// SPDX-License-Identifier: GPL-2.0
/*
 * abi_p9net - the Plan 9 network file system (/net) on Linux sockets
 *
 *	mount -t p9net none /net
 *
 * provides
 *
 *	/net/cs			connection server: "tcp!host!port" ->
 *				"/net/tcp/clone 10.0.0.1!80"
 *	/net/tcp/clone		opening it allocates a conversation;
 *	/net/udp/clone		reading it returns the conversation number
 *	/net/tcp/N/ctl		connect addr!port, announce [addr!]port, hangup
 *	/net/tcp/N/data		the byte stream
 *	/net/tcp/N/listen	opening it waits for a call and returns the
 *				ctl file of a new conversation
 *	/net/tcp/N/local	"addr!port"
 *	/net/tcp/N/remote
 *	/net/tcp/N/status
 *
 * which is what dial(2), announce(2) and listen(2) in the Plan 9 C
 * library use.  The layout follows the netfs skeleton from Glendix
 * (Rahul Murmuria, 2009).  Sockets are created in the network name
 * space of the process that opens the clone file.
 */
#define pr_fmt(fmt) "p9net: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/fs_context.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/inet.h>
#include <linux/uio.h>
#include <linux/socket.h>
#include <linux/dns_resolver.h>
#include <linux/nsproxy.h>
#include <net/sock.h>
#include <net/net_namespace.h>

#define P9NET_MAGIC	0x19980122	/* the one Glendix used */
#define MAXCONV		256

enum { F_CTL, F_DATA, F_LISTEN, F_LOCAL, F_REMOTE, F_STATUS, F_CLONE };
static const char *const conv_files[] = {
	[F_CTL] = "ctl", [F_DATA] = "data", [F_LISTEN] = "listen",
	[F_LOCAL] = "local", [F_REMOTE] = "remote", [F_STATUS] = "status",
};

enum { S_FREE, S_IDLE, S_CONNECTED, S_ANNOUNCED, S_CLOSED };
static const char *const state_names[] = {
	"Closed", "Idle", "Established", "Listen", "Closed",
};

struct p9proto;

struct conv {
	struct p9proto	*proto;
	int		id;
	int		ref;		/* open files */
	int		state;
	struct mutex	lock;
	struct socket	*sock;
	struct dentry	*dir;
};

struct p9proto {
	const char	*name;
	int		type;		/* SOCK_STREAM, SOCK_DGRAM */
	int		ipproto;
	struct conv	*conv[MAXCONV];
	struct dentry	*dir;
};

static struct p9proto protos[] = {
	{ .name = "tcp", .type = SOCK_STREAM, .ipproto = IPPROTO_TCP },
	{ .name = "udp", .type = SOCK_DGRAM, .ipproto = IPPROTO_UDP },
};

static DEFINE_MUTEX(p9net_lock);	/* conversation tables and dentries */
static struct super_block *p9net_sb;

/*
 * Addresses: "ip!port", "*!port", "port"
 */
static int parse_addr(const char *s, struct sockaddr_storage *ss, int *len)
{
	struct sockaddr_in *sin = (struct sockaddr_in *)ss;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ss;
	char host[64];
	const char *bang = strrchr(s, '!');
	unsigned int port;

	memset(ss, 0, sizeof(*ss));
	if (!bang) {
		strscpy(host, "*", sizeof(host));
		bang = s - 1;
	} else {
		if (bang - s >= sizeof(host))
			return -EINVAL;
		memcpy(host, s, bang - s);
		host[bang - s] = 0;
	}
	if (kstrtouint(bang + 1, 10, &port) || port > 65535)
		return -EINVAL;
	if (!strcmp(host, "*") || !host[0]) {
		sin->sin_family = AF_INET;
		sin->sin_addr.s_addr = htonl(INADDR_ANY);
		sin->sin_port = htons(port);
		*len = sizeof(*sin);
		return 0;
	}
	if (in4_pton(host, -1, (u8 *)&sin->sin_addr, -1, NULL)) {
		sin->sin_family = AF_INET;
		sin->sin_port = htons(port);
		*len = sizeof(*sin);
		return 0;
	}
	if (in6_pton(host, -1, (u8 *)&sin6->sin6_addr, -1, NULL)) {
		sin6->sin6_family = AF_INET6;
		sin6->sin6_port = htons(port);
		*len = sizeof(*sin6);
		return 0;
	}
	return -EINVAL;
}

static int format_addr(struct sockaddr_storage *ss, char *buf, size_t n)
{
	if (ss->ss_family == AF_INET6) {
		struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)ss;

		return scnprintf(buf, n, "%pI6c!%u\n", &s6->sin6_addr,
				 ntohs(s6->sin6_port));
	}
	if (ss->ss_family == AF_INET) {
		struct sockaddr_in *s4 = (struct sockaddr_in *)ss;

		return scnprintf(buf, n, "%pI4!%u\n", &s4->sin_addr,
				 ntohs(s4->sin_port));
	}
	return scnprintf(buf, n, "::!0\n");
}

/*
 * Inodes and dentries
 */
static struct inode *p9net_inode(struct super_block *sb, umode_t mode,
				 const struct file_operations *fops, void *priv)
{
	struct inode *inode = new_inode(sb);

	if (!inode)
		return NULL;
	inode->i_ino = get_next_ino();
	inode->i_mode = mode;
	simple_inode_init_ts(inode);
	inode->i_private = priv;
	if (S_ISDIR(mode)) {
		inode->i_op = &simple_dir_inode_operations;
		inode->i_fop = &simple_dir_operations;
		set_nlink(inode, 2);
	} else {
		inode->i_fop = fops;
	}
	return inode;
}

static struct dentry *p9net_add(struct dentry *parent, const char *name,
				umode_t mode, const struct file_operations *fops,
				void *priv)
{
	struct inode *dir = d_inode(parent), *inode;
	struct dentry *d;

	inode_lock(dir);
	d = d_alloc_name(parent, name);
	if (!d)
		goto out;
	inode = p9net_inode(parent->d_sb, mode, fops, priv);
	if (!inode) {
		dput(d);
		d = NULL;
		goto out;
	}
	if (S_ISDIR(mode))
		inc_nlink(dir);
	d_add(d, inode);		/* the reference stays with the tree */
out:
	inode_unlock(dir);
	return d;
}

/* file ops forward declarations */
static const struct file_operations conv_fops[F_CLONE + 1];
static const struct file_operations cs_fops;

static int conv_mkdir(struct conv *c)
{
	char name[12];
	int i;

	if (!p9net_sb || !c->proto->dir || c->dir)
		return 0;
	snprintf(name, sizeof(name), "%d", c->id);
	c->dir = p9net_add(c->proto->dir, name, S_IFDIR | 0555, NULL, NULL);
	if (!c->dir)
		return -ENOMEM;
	for (i = 0; i < ARRAY_SIZE(conv_files); i++)
		p9net_add(c->dir, conv_files[i], S_IFREG | 0666, &conv_fops[i], c);
	return 0;
}

/*
 * Conversations
 */
static void conv_hangup(struct conv *c)
{
	if (c->sock) {
		kernel_sock_shutdown(c->sock, SHUT_RDWR);
		sock_release(c->sock);
		c->sock = NULL;
	}
	c->state = S_CLOSED;
}

/* Allocate (or reuse) a conversation; called with p9net_lock held. */
static struct conv *conv_alloc(struct p9proto *p)
{
	struct conv *c;
	int i;

	for (i = 0; i < MAXCONV; i++) {
		c = p->conv[i];
		if (!c) {
			c = kzalloc(sizeof(*c), GFP_KERNEL);
			if (!c)
				return NULL;
			c->proto = p;
			c->id = i;
			mutex_init(&c->lock);
			p->conv[i] = c;
			conv_mkdir(c);
		}
		mutex_lock(&c->lock);
		if (c->ref == 0) {
			c->ref = 1;
			c->state = S_IDLE;
			mutex_unlock(&c->lock);
			return c;
		}
		mutex_unlock(&c->lock);
	}
	return NULL;
}

static void conv_put(struct conv *c)
{
	mutex_lock(&c->lock);
	if (--c->ref == 0) {
		conv_hangup(c);
		c->state = S_FREE;
	}
	mutex_unlock(&c->lock);
}

static int conv_socket(struct conv *c, int family)
{
	if (c->sock && c->sock->sk->sk_family == family)
		return 0;
	if (c->sock)
		sock_release(c->sock);
	c->sock = NULL;
	return sock_create(family, c->proto->type, c->proto->ipproto, &c->sock);
}

static int conv_ctl(struct conv *c, char *cmd)
{
	struct sockaddr_storage ss;
	char *arg;
	int err, len;

	cmd = strim(cmd);
	arg = strchr(cmd, ' ');
	if (arg)
		*arg++ = 0;

	if (!strcmp(cmd, "hangup")) {
		conv_hangup(c);
		return 0;
	}
	if (!strcmp(cmd, "connect") && arg) {
		char *local = NULL, *bang;

		/* connect addr!port[!r] [local]: drop the options */
		local = strchr(arg, ' ');
		if (local)
			*local = 0;
		bang = strchr(arg, '!');
		if (bang && strchr(bang + 1, '!'))
			*strchr(bang + 1, '!') = 0;
		err = parse_addr(arg, &ss, &len);
		if (err)
			return err;
		err = conv_socket(c, ss.ss_family);
		if (err)
			return err;
		err = kernel_connect(c->sock, (struct sockaddr *)&ss, len, 0);
		if (!err)
			c->state = S_CONNECTED;
		return err;
	}
	if (!strcmp(cmd, "announce") && arg) {
		err = parse_addr(arg, &ss, &len);
		if (err)
			return err;
		err = conv_socket(c, ss.ss_family);
		if (err)
			return err;
		sock_set_reuseaddr(c->sock->sk);
		err = kernel_bind(c->sock, (struct sockaddr *)&ss, len);
		if (!err && c->proto->type == SOCK_STREAM)
			err = kernel_listen(c->sock, 16);
		if (!err)
			c->state = S_ANNOUNCED;
		return err;
	}
	if (!strcmp(cmd, "bind") || !strcmp(cmd, "keepalive") ||
	    !strcmp(cmd, "ttl") || !strcmp(cmd, "tos") ||
	    !strcmp(cmd, "headers"))
		return 0;	/* accepted and ignored */
	return -EINVAL;
}

/*
 * Files of a conversation
 */
static int conv_open(struct inode *inode, struct file *f)
{
	struct conv *c = inode->i_private;

	mutex_lock(&c->lock);
	if (c->ref == 0) {
		mutex_unlock(&c->lock);
		return -ENOENT;		/* not in use: open clone instead */
	}
	c->ref++;
	mutex_unlock(&c->lock);
	f->private_data = c;
	return 0;
}

static int conv_release(struct inode *inode, struct file *f)
{
	conv_put(f->private_data);
	return 0;
}

static ssize_t read_string(struct file *f, char __user *buf, size_t n,
			   loff_t *pos, const char *s)
{
	return simple_read_from_buffer(buf, n, pos, s, strlen(s));
}

/* ctl (and clone, and listen): reading gives the conversation number */
static ssize_t ctl_read(struct file *f, char __user *buf, size_t n, loff_t *pos)
{
	struct conv *c = f->private_data;
	char s[12];

	snprintf(s, sizeof(s), "%d", c->id);
	return read_string(f, buf, n, pos, s);
}

static ssize_t ctl_write(struct file *f, const char __user *buf, size_t n,
			 loff_t *pos)
{
	struct conv *c = f->private_data;
	char *cmd;
	int err;

	if (n > 512)
		return -EINVAL;
	cmd = memdup_user_nul(buf, n);
	if (IS_ERR(cmd))
		return PTR_ERR(cmd);
	mutex_lock(&c->lock);
	err = conv_ctl(c, cmd);
	mutex_unlock(&c->lock);
	kfree(cmd);
	return err ?: n;
}

static ssize_t data_read(struct file *f, char __user *buf, size_t n,
			 loff_t *pos)
{
	struct conv *c = f->private_data;
	struct msghdr msg = { };
	int err;

	if (!c->sock || c->state == S_CLOSED)
		return 0;
	err = import_ubuf(ITER_DEST, buf, n, &msg.msg_iter);
	if (err)
		return err;
	return sock_recvmsg(c->sock, &msg, 0);
}

static ssize_t data_write(struct file *f, const char __user *buf, size_t n,
			  loff_t *pos)
{
	struct conv *c = f->private_data;
	struct msghdr msg = { .msg_flags = MSG_NOSIGNAL };
	int err;

	if (!c->sock || c->state != S_CONNECTED)
		return -EPIPE;
	err = import_ubuf(ITER_SOURCE, (void __user *)buf, n, &msg.msg_iter);
	if (err)
		return err;
	return sock_sendmsg(c->sock, &msg);
}

static ssize_t addr_read(struct file *f, char __user *buf, size_t n,
			 loff_t *pos, bool peer)
{
	struct conv *c = f->private_data;
	struct sockaddr_storage ss = { };
	char s[80];
	int err = -ENOTCONN;

	mutex_lock(&c->lock);
	if (c->sock)
		err = peer ? kernel_getpeername(c->sock, (struct sockaddr *)&ss) :
			     kernel_getsockname(c->sock, (struct sockaddr *)&ss);
	mutex_unlock(&c->lock);
	if (err < 0)
		ss.ss_family = AF_UNSPEC;
	format_addr(&ss, s, sizeof(s));
	return read_string(f, buf, n, pos, s);
}

static ssize_t laddr_read(struct file *f, char __user *b, size_t n, loff_t *p)
{
	return addr_read(f, b, n, p, false);
}

static ssize_t raddr_read(struct file *f, char __user *b, size_t n, loff_t *p)
{
	return addr_read(f, b, n, p, true);
}

static ssize_t status_read(struct file *f, char __user *buf, size_t n,
			   loff_t *pos)
{
	struct conv *c = f->private_data;
	char s[32];

	snprintf(s, sizeof(s), "%s\n", state_names[c->state]);
	return read_string(f, buf, n, pos, s);
}

/*
 * listen: wait for a call on an announced conversation; the open file
 * becomes the ctl file of a new conversation holding the call.
 */
static int listen_open(struct inode *inode, struct file *f)
{
	struct conv *c = inode->i_private, *nc;
	struct socket *sock, *nsock;
	int err;

	mutex_lock(&c->lock);
	if (c->ref == 0 || c->state != S_ANNOUNCED || !c->sock) {
		mutex_unlock(&c->lock);
		return -EINVAL;
	}
	c->ref++;			/* keep the socket while we wait */
	sock = c->sock;
	mutex_unlock(&c->lock);

	err = kernel_accept(sock, &nsock, 0);
	conv_put(c);
	if (err)
		return err;

	mutex_lock(&p9net_lock);
	nc = conv_alloc(c->proto);
	mutex_unlock(&p9net_lock);
	if (!nc) {
		sock_release(nsock);
		return -ENFILE;
	}
	mutex_lock(&nc->lock);
	nc->sock = nsock;
	nc->state = S_CONNECTED;
	mutex_unlock(&nc->lock);
	f->private_data = nc;
	return 0;
}

static int clone_open(struct inode *inode, struct file *f)
{
	struct p9proto *p = inode->i_private;
	struct conv *c;

	mutex_lock(&p9net_lock);
	c = conv_alloc(p);
	mutex_unlock(&p9net_lock);
	if (!c)
		return -ENFILE;
	f->private_data = c;
	return 0;
}

static const struct file_operations conv_fops[F_CLONE + 1] = {
	[F_CTL] = { .owner = THIS_MODULE, .open = conv_open,
		    .release = conv_release, .read = ctl_read,
		    .write = ctl_write, .llseek = default_llseek },
	[F_DATA] = { .owner = THIS_MODULE, .open = conv_open,
		     .release = conv_release, .read = data_read,
		     .write = data_write, .llseek = noop_llseek },
	[F_LISTEN] = { .owner = THIS_MODULE, .open = listen_open,
		       .release = conv_release, .read = ctl_read,
		       .write = ctl_write, .llseek = default_llseek },
	[F_LOCAL] = { .owner = THIS_MODULE, .open = conv_open,
		      .release = conv_release, .read = laddr_read,
		      .llseek = default_llseek },
	[F_REMOTE] = { .owner = THIS_MODULE, .open = conv_open,
		       .release = conv_release, .read = raddr_read,
		       .llseek = default_llseek },
	[F_STATUS] = { .owner = THIS_MODULE, .open = conv_open,
		       .release = conv_release, .read = status_read,
		       .llseek = default_llseek },
	[F_CLONE] = { .owner = THIS_MODULE, .open = clone_open,
		      .release = conv_release, .read = ctl_read,
		      .write = ctl_write, .llseek = default_llseek },
};

/*
 * The connection server: write "proto!host!service", then read
 * "/net/proto/clone addr!port" lines.
 */
static const struct { const char *name; int port; } services[] = {
	{ "echo", 7 }, { "discard", 9 }, { "daytime", 13 }, { "ftp", 21 },
	{ "ssh", 22 }, { "telnet", 23 }, { "smtp", 25 }, { "domain", 53 },
	{ "dns", 53 }, { "http", 80 }, { "pop3", 110 }, { "ntp", 123 },
	{ "imap", 143 }, { "https", 443 }, { "9fs", 564 }, { "rexexec", 17009 },
	{ "ncpu", 17010 }, { "cpu", 17013 }, { "exportfs", 17007 },
	{ "venti", 17034 },
};

struct cs_state {
	char	answer[256];
};

static int cs_lookup(const char *req, char *out, size_t n)
{
	char buf[256], *proto, *host, *svc, *addr = NULL;
	unsigned int port = 0;
	int i, err;

	strscpy(buf, req, sizeof(buf));
	strim(buf);
	proto = buf;
	host = strchr(proto, '!');
	if (!host)
		return -EINVAL;
	*host++ = 0;
	svc = strchr(host, '!');
	if (svc)
		*svc++ = 0;
	if (!strcmp(proto, "net"))
		proto = "tcp";
	if (strcmp(proto, "tcp") && strcmp(proto, "udp"))
		return -EPFNOSUPPORT;

	if (svc && kstrtouint(svc, 10, &port)) {
		for (i = 0; i < ARRAY_SIZE(services); i++)
			if (!strcmp(svc, services[i].name))
				port = services[i].port;
		if (!port)
			return -ENOENT;
	}

	if (!strcmp(host, "*")) {
		snprintf(out, n, "/net/%s/clone %u", proto, port);
		return 0;
	}
	if (!strcmp(host, "localhost") || !strcmp(host, "loopback"))
		host = "127.0.0.1";
	{
		u8 a[16];

		if (in4_pton(host, -1, a, -1, NULL) ||
		    in6_pton(host, -1, a, -1, NULL)) {
			snprintf(out, n, "/net/%s/clone %s!%u", proto, host, port);
			return 0;
		}
	}
	/* ask the kernel DNS resolver (needs a request-key helper) */
	err = dns_query(current->nsproxy->net_ns, NULL, host, strlen(host),
			NULL, &addr, NULL, false);
	if (err < 0)
		return -EHOSTUNREACH;
	snprintf(out, n, "/net/%s/clone %s!%u", proto, addr, port);
	kfree(addr);
	return 0;
}

static int cs_open(struct inode *inode, struct file *f)
{
	f->private_data = kzalloc(sizeof(struct cs_state), GFP_KERNEL);
	return f->private_data ? 0 : -ENOMEM;
}

static int cs_release(struct inode *inode, struct file *f)
{
	kfree(f->private_data);
	return 0;
}

static ssize_t cs_write(struct file *f, const char __user *buf, size_t n,
			loff_t *pos)
{
	struct cs_state *cs = f->private_data;
	char *req;
	int err;

	if (n > 255)
		return -EINVAL;
	req = memdup_user_nul(buf, n);
	if (IS_ERR(req))
		return PTR_ERR(req);
	err = cs_lookup(req, cs->answer, sizeof(cs->answer));
	kfree(req);
	if (err) {
		cs->answer[0] = 0;
		return err;
	}
	*pos = 0;
	return n;
}

static ssize_t cs_read(struct file *f, char __user *buf, size_t n, loff_t *pos)
{
	struct cs_state *cs = f->private_data;

	return read_string(f, buf, n, pos, cs->answer);
}

static const struct file_operations cs_fops = {
	.owner = THIS_MODULE, .open = cs_open, .release = cs_release,
	.read = cs_read, .write = cs_write, .llseek = default_llseek,
};

/*
 * The file system
 */
static const struct super_operations p9net_sops = {
	.statfs		= simple_statfs,
	.drop_inode	= generic_delete_inode,
};

static int p9net_fill_super(struct super_block *sb, struct fs_context *fc)
{
	struct inode *root;
	int i, j;

	sb->s_blocksize = PAGE_SIZE;
	sb->s_blocksize_bits = PAGE_SHIFT;
	sb->s_magic = P9NET_MAGIC;
	sb->s_op = &p9net_sops;
	sb->s_time_gran = 1;
	root = p9net_inode(sb, S_IFDIR | 0555, NULL, NULL);
	if (!root)
		return -ENOMEM;
	sb->s_root = d_make_root(root);
	if (!sb->s_root)
		return -ENOMEM;

	mutex_lock(&p9net_lock);
	p9net_sb = sb;
	p9net_add(sb->s_root, "cs", S_IFREG | 0666, &cs_fops, NULL);
	for (i = 0; i < ARRAY_SIZE(protos); i++) {
		struct p9proto *p = &protos[i];

		p->dir = p9net_add(sb->s_root, p->name, S_IFDIR | 0555, NULL, NULL);
		if (!p->dir)
			continue;
		p9net_add(p->dir, "clone", S_IFREG | 0666, &conv_fops[F_CLONE], p);
		for (j = 0; j < MAXCONV; j++)
			if (p->conv[j])
				conv_mkdir(p->conv[j]);
	}
	mutex_unlock(&p9net_lock);
	return 0;
}

static void p9net_kill_sb(struct super_block *sb)
{
	int i, j;

	mutex_lock(&p9net_lock);
	p9net_sb = NULL;
	for (i = 0; i < ARRAY_SIZE(protos); i++) {
		protos[i].dir = NULL;
		for (j = 0; j < MAXCONV; j++)
			if (protos[i].conv[j])
				protos[i].conv[j]->dir = NULL;
	}
	mutex_unlock(&p9net_lock);
	kill_litter_super(sb);
}

static int p9net_get_tree(struct fs_context *fc)
{
	return get_tree_single(fc, p9net_fill_super);
}

static const struct fs_context_operations p9net_context_ops = {
	.get_tree = p9net_get_tree,
};

static int p9net_init_fs_context(struct fs_context *fc)
{
	fc->ops = &p9net_context_ops;
	return 0;
}

static struct file_system_type p9net_fs_type = {
	.owner			= THIS_MODULE,
	.name			= "p9net",
	.init_fs_context	= p9net_init_fs_context,
	.kill_sb		= p9net_kill_sb,
};

static int __init p9net_init(void)
{
	return register_filesystem(&p9net_fs_type);
}

static void __exit p9net_exit(void)
{
	int i, j;

	unregister_filesystem(&p9net_fs_type);
	for (i = 0; i < ARRAY_SIZE(protos); i++)
		for (j = 0; j < MAXCONV; j++)
			kfree(protos[i].conv[j]);
}

module_init(p9net_init);
module_exit(p9net_exit);

MODULE_ALIAS_FS("p9net");
MODULE_DESCRIPTION("abiemu: Plan 9 /net on Linux sockets");
MODULE_AUTHOR("linux-abiemu contributors; layout after Glendix netfs");
MODULE_LICENSE("GPL");
