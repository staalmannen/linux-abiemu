#ident "%W% %G%"

#include <linux/string.h>
#include <linux/fs.h>
#include <linux/slab.h> /* needed by putname macro */
#include <linux/sched.h> /* needed by current-> in __abi_trace() macro */
#include <linux/module.h>

#include <abi/util/trace.h>


#if defined(CONFIG_ABI_TRACE)
static void
print_string(char *buf, char *str)
{
	char *tmp;

	tmp = getname(str);
	if (!IS_ERR(tmp)) {
		/* we are debugging, we don't need to see it all */
		tmp[80] = '\0';
		sprintf(buf, "\"%s\"", tmp);
		putname(tmp);
	}
}

void plist(char *name, char *args, int *list)
{
	char buf[512], *p = buf;

	buf[0] = '\0';
	while (*args) {
		switch (*args++) {
		case 'd':
			sprintf(p, "%d", *list++);
			break;
		case 'o':
			sprintf(p, "0%o", *list++);
			break;
		case 'p':
			sprintf(p, "%p", (void *)(*list++));
			break;
		case '?':
		case 'x':
			sprintf(p, "0x%x", *list++);
			break;
		case 's':
			print_string(p, (char *)(*list++));
			break;
		default:
			sprintf(p, "?%c%c?", '%', args[-1]);
			break;
		}

		while (*p)
			++p;
		if (*args) {
			*p++ = ',';
			*p++ = ' ';
			*p = '\0';
		}
	}
	__abi_trace("%s(%s)\n", name, buf);
}

#if CONFIG_ABI_LCALL7 == m
EXPORT_SYMBOL(plist);
#endif
#endif /* CONFIG_ABI_TRACE */
