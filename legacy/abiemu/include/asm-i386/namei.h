/* $Id: namei.h 25 2006-01-23 22:53:17Z fwenzel $
 * linux/include/asm-i386/namei.h
 *
 * Included from linux/fs/namei.c
 */

#ifndef __I386_NAMEI_H
#define __I386_NAMEI_H

#include <linux/config.h>

/*
 * The base directory for our emulations.
 *  - sparc uses usr/gmemul here.
 */
#define I386_EMUL_BASE		"emul/"

/*
 * We emulate quite a lot operting systems...
 */
#define I386_SVR4_EMUL		I386_EMUL_BASE "/svr4/"
#define I386_SVR3_EMUL		I386_EMUL_BASE "/svr3/"
#define I386_SCOSVR3_EMUL	I386_EMUL_BASE "/sco/"
#define I386_OSR5_EMUL		I386_EMUL_BASE "/osr5/"
#define I386_WYSEV386_EMUL	I386_EMUL_BASE "/wyse/"
#define I386_ISCR4_EMUL		I386_EMUL_BASE "/isc/"
#define I386_BSD_EMUL		I386_EMUL_BASE "/bsd/"
#define I386_XENIX_EMUL		I386_EMUL_BASE "/xenix/"
#define I386_SOLARIS_EMUL	I386_EMUL_BASE "/solaris/"
#define I386_UW7_EMUL		I386_EMUL_BASE "/uw7/"

static inline char *__emul_prefix(void)
{
#if defined(CONFIG_ABI) || defined (CONFIG_ABI_MODULE)
	switch (get_cur_personality()) {
	case PER_SVR4:
		return I386_SVR4_EMUL;
	case PER_SVR3:
		return I386_SVR3_EMUL;
	case PER_SCOSVR3:
		return I386_SCOSVR3_EMUL;
	case PER_OSR5:
		return I386_OSR5_EMUL;
	case PER_WYSEV386:
		return I386_WYSEV386_EMUL;
	case PER_ISCR4:
		return I386_ISCR4_EMUL;
	case PER_BSD:
		return I386_BSD_EMUL;
	case PER_XENIX:
		return I386_XENIX_EMUL;
	case PER_SOLARIS:
		return I386_SOLARIS_EMUL;
	case PER_UW7:
		return I386_UW7_EMUL;
	}
#endif /* CONFIG_ABI || CONFIG_ABI_MODULE */
	return NULL;
}

#endif /* __I386_NAMEI_H */
