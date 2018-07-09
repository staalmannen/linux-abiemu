#ifndef _LINUX_PERSONALITY_H
#define _LINUX_PERSONALITY_H

/*
 * Handling of different ABIs (personalities).
 */

struct exec_domain;
struct pt_regs;

typedef unsigned long personality_t;  /* type for full personality value */
typedef unsigned char per_id_t;/* type for base per id */

extern int		register_exec_domain(struct exec_domain *);
extern int		unregister_exec_domain(struct exec_domain *);
extern int		__set_personality(unsigned long);

/*
 * Sysctl variables related to binary emulation.
 */
extern unsigned long abi_defhandler_coff;
extern unsigned long abi_defhandler_elf;
extern unsigned long abi_defhandler_lcall7;
extern unsigned long abi_defhandler_libcso;
extern int abi_fake_utsname;

/*
 * Basic personality ids (per_id).
 *
 * The per_id is occupying byte 0.
 * (It has to fit into an unsigned char! -> per_id_t)
 */
enum {
      PERID_LINUX =           0x00, /* native execution mode of os platform */
      PERID_SVR4 =            0x01,
      PERID_SVR3 =            0x02,
      PERID_SCOSVR3 =         0x03, /* and SCO-OS-R5 */
      PERID_WYSEV386 =        0x04,
      PERID_ISCR4 =           0x05,
      PERID_BSD =             0x06, /* and SUN OS */
      PERID_XENIX =           0x07,
      PERID_LINUX32 =         0x08, /* 32 bit mode on non-32 bit platforms */
      PERID_IRIX32 =          0x09, /* IRIX5 32-bit */
      PERID_IRIXN32 =         0x0a, /* IRIX6 new 32-bit */
      PERID_IRIX64 =          0x0b, /* IRIX6 64-bit */
      PERID_RISCOS =          0x0c,
      PERID_SOLARIS =         0x0d,
      PERID_UW7 =             0x0e,
      PERID_OSF4 =            0x0f, /* OSF/1 v4 */
      PERID_HPUX =            0x10,
      PERID_MASK =            0xff,
};
  
/*
 * Flags for customizing the behaviour of a base personality id.
 *
 * The flags occupy bytes 1 trough 3. Byte 0 is for the base personality id.
 * (Avoid defining the top bit, it will conflict with error returns.)
 */
enum {
#if 1 /* only for compatibilty with old code */	
	ADDR_NO_RANDOMIZE = 	0x0040000,	/* disable randomization of VA space */
	FDPIC_FUNCPTRS =	0x0080000,	/* userspace function ptrs point to descriptors
						 * (signal handling)
						 */
	MMAP_PAGE_ZERO =	0x0100000,
	ADDR_COMPAT_LAYOUT =	0x0200000,
	READ_IMPLIES_EXEC =	0x0400000,
	ADDR_LIMIT_32BIT =	0x0800000,
	SHORT_INODE =		0x1000000,
	WHOLE_SECONDS =		0x2000000,
	STICKY_TIMEOUTS =       0x4000000,
	ADDR_LIMIT_3GB = 	0x8000000,
#endif
        /*                              ffffffpp */
	PERF_MMAP_PAGE_ZERO =           0x00100000,
	PERF_ADDR_COMPAT_LAYOUT =       0x00200000,
	PERF_READ_IMPLIES_EXEC =        0x00400000,
	PERF_ADDR_LIMIT_32BIT =         0x00800000,
	PERF_SHORT_INODE =              0x01000000,
	PERF_WHOLE_SECONDS =            0x02000000,
	PERF_STICKY_TIMEOUTS =          0x04000000,
	PERF_ADDR_LIMIT_3GB =           0x08000000,
	PERF_MASK =                     0xFFFFFF00,
};

/*
 * Security-relevant compatibility flags that must be
 * cleared upon setuid or setgid exec:
 */
#define PER_CLEAR_ON_SETID (READ_IMPLIES_EXEC|ADDR_NO_RANDOMIZE)

/*
 * Predefined personality profiles.
 */
enum {
	PER_LINUX =             PERID_LINUX,
	PER_LINUX_32BIT =       PERID_LINUX    | PERF_ADDR_LIMIT_32BIT,
	PER_SVR4 =              PERID_SVR4     | PERF_STICKY_TIMEOUTS | PERF_MMAP_PAGE_ZERO,
	PER_SVR3 =              PERID_SVR3     | PERF_STICKY_TIMEOUTS | PERF_SHORT_INODE,
	PER_SCOSVR3 =           PERID_SCOSVR3  | PERF_STICKY_TIMEOUTS |
                                                 PERF_WHOLE_SECONDS | PERF_SHORT_INODE,
	PER_LINUX_FDPIC =	0x0000 | FDPIC_FUNCPTRS,
	PER_OSR5 =              PERID_SCOSVR3  | PERF_STICKY_TIMEOUTS | PERF_WHOLE_SECONDS,
	PER_WYSEV386 =          PERID_WYSEV386 | PERF_STICKY_TIMEOUTS | PERF_SHORT_INODE,
	PER_ISCR4 =             PERID_ISCR4    | PERF_STICKY_TIMEOUTS,
	PER_BSD =               PERID_BSD,
	PER_SUNOS =             PERID_BSD      | PERF_STICKY_TIMEOUTS,
	PER_XENIX =             PERID_XENIX    | PERF_STICKY_TIMEOUTS | PERF_SHORT_INODE,
	PER_LINUX32 =           PERID_LINUX32,
	PER_LINUX32_3GB =       PERID_LINUX32  | PERF_ADDR_LIMIT_3GB,
	PER_IRIX32 =            PERID_IRIX32   | PERF_STICKY_TIMEOUTS,/* IRIX5 32-bit */
	PER_IRIXN32 =           PERID_IRIXN32  | PERF_STICKY_TIMEOUTS,/* IRIX6 new 32-bit */
	PER_IRIX64 =            PERID_IRIX64   | PERF_STICKY_TIMEOUTS,/* IRIX6 64-bit */
	PER_RISCOS =            PERID_RISCOS,
	PER_SOLARIS =           PERID_SOLARIS  | PERF_STICKY_TIMEOUTS,
	PER_UW7 =               PERID_UW7      | PERF_STICKY_TIMEOUTS | PERF_MMAP_PAGE_ZERO,
	PER_OSF4 =              PERID_OSF4,/* OSF/1 v4 */
	PER_HPUX =              PERID_HPUX,
	PER_MASK =              PERID_MASK     | PERF_MASK,
	PER_QUERY =             0xffffffff, /* indicates query when passed to sys_personality */
};


/*
 * Description of an execution domain.
 * 
 * The first two members are refernced from assembly source
 * and should stay where they are unless explicitly needed.
 */
typedef void (*handler_t)(int, struct pt_regs *);

struct exec_domain {
	const char		*name;		/* name of the execdomain */
	handler_t		handler;	/* handler for syscalls */
	per_id_t		pers_low;	/* lowest personality id for this execdomain */
	per_id_t		pers_high;	/* highest personality id for this execdomain */
	unsigned long		*signal_map;	/* signal mapping */
	unsigned long		*signal_invmap;	/* reverse signal mapping */
	struct map_segment	*err_map;	/* error mapping */
	struct map_segment	*socktype_map;	/* socket type mapping */
	struct map_segment	*sockopt_map;	/* socket option mapping */
	struct map_segment	*af_map;	/* address family mapping */
	struct module		*module;	/* module context of the ed. */
	struct exec_domain	*next;		/* linked list (internal) */
};

/*
 * Return the base personality id of the given personality.
 */
#define get_personality_id(personality)         ((personality) & PERID_MASK)
/* compatibility with old naming */
#define personality(personality)                get_personality_id(personality)

/*
 * Return the flags of the given personality.
 */
#define get_personality_flags(personality)      ((personality) & PERF_MASK)

/*
 * Personality of the currently running process.
 */
#define get_cur_personality()		(current->personality)

/*
 * Return the base personality id of current process.
 */
#define get_cur_personality_id()	get_personality_id(get_cur_personality())

/*
 * Return the flags of the personality of current process.
 */
#define get_cur_personality_flags()	get_personality_flags(get_cur_personality())

/*
 * Check if the personality of the current process matches with the given value.
 */
#define is_cur_personality(personality) \
        (get_cur_personality() == (personality))

/*
 * Check if the pers id of the current userland process matches the given.
 * No masking does apply to the given id!
 */
#define is_cur_personality_id(per_id) \
        (get_cur_personality_id() == (per_id))

/*
 * Check if the personality id of the current userland process
 * matches the personality id of the given personality code value.
 */
#define is_cur_same_personality_id_as(personality) \
        (get_cur_personality_id() == get_personality_id(personality))

/*
 * Check if the current userland process has set one or more of the given flags.
 * This macro works only with single flags!
 */
#define is_cur_personality_flag(per_flag) \
        (get_cur_personality_flags() & (per_flag))

/*
 * Check if the current userland process has all of the given flags set.
 * This macro works only with single flags!
 */
#define is_cur_personality_all_flags(per_flags) \
        (get_cur_personality_flags() & (per_flags) == (per_flags))

/*
 * Set a new personality for the current userland process (if not already set).
 */
#define set_cur_personality(personality) \
        (is_cur_personality(personality) ? 0 : __set_personality(personality))
/* compatibility with old naming */
#define set_personality(personality) set_cur_personality(personality)

#endif /* _LINUX_PERSONALITY_H */
