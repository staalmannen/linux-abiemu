#ifndef _ABI_SOLARIS_SYSENT_H
#define _ABI_SOLARIS_SYSENT_H

#ident "%W% %G%"

struct sol_stat64;
struct sol_nmsghdr;
struct sockaddr;


/* lfs.c */
extern int	sol_open64(const char *, int, int);
extern int	sol_getdents64(int fd, char *, int);
extern int	sol_mmap64(u_int, u_int, int, int, int, u_int, u_int);

/* socket.c */
extern int	solaris_socket(int family, int type, int protocol);
extern int	solaris_socketpair(int *usockvec);
extern int	solaris_bind(int fd, struct sockaddr *addr, int addrlen);
extern int	solaris_setsockopt(int fd, int level, int optname,
			u32 optval, int optlen);
extern int	solaris_getsockopt(int fd, int level, int optname,
			u32 optval, u32 optlen);
extern int	solaris_connect(int fd, struct sockaddr *addr, int addrlen);
extern int	solaris_accept(int fd, struct sockaddr *addr, int *addrlen);
extern int	solaris_listen(int fd, int backlog);
extern int	solaris_shutdown(int fd, int how);
extern int	solaris_recvfrom(int s, char *buf, int len, int flags,
			u32 from, u32 fromlen);
extern int	solaris_recv(int s, char *buf, int len, int flags);
extern int	solaris_sendto(int s, char *buf, int len, int flags,
			u32 to, u32 tolen);
extern int	solaris_send(int s, char *buf, int len, int flags);
extern int	solaris_getpeername(int fd, struct sockaddr *addr,
			int *addrlen);
extern int	solaris_getsockname(int fd, struct sockaddr *addr,
			int *addrlen);
extern int	solaris_sendmsg(int fd, struct sol_nmsghdr *user_msg,
			unsigned user_flags);
extern int	solaris_recvmsg(int fd, struct sol_nmsghdr *user_msg,
			unsigned user_flags);

/* solarisx86.c */
extern int	sol_llseek(struct pt_regs *regs);
extern int	sol_memcntl(unsigned addr, unsigned len, int cmd,
			unsigned arg, int attr, int mask);
extern int	sol_acl(char *pathp, int cmd, int nentries, void *aclbufp);

/* stat.c */
extern int	sol_stat64(char *, struct sol_stat64 *);
extern int	sol_lstat64(char *, struct sol_stat64 *);
extern int	sol_fstat64(u_int fd, struct sol_stat64 *);

#endif /* _ABI_SOLARIS_SYSENT_H */
