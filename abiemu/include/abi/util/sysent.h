#ifndef _ABI_SYSENT_H
#define _ABI_SYSENT_H

#ident "%W% %G%"


#include <asm/abi_machdep.h>

struct pt_regs;
struct sysent;

/*
 * This is needed for subdispatchers like cxenix().
 */
extern void	lcall7_dispatch(struct pt_regs *, struct sysent *, int);


/*
 *  - If an entry is 'Ukn' we don't know how to handle it yet.
 *  - Spl means that we need to do special processing for this syscall.
 *  - Fast means that even the error return handling is done by the function.
 *  - Unimpl means the syscall is not implemented at all.
 */
enum {
	Spl = 0x65,	/* pass the regs structure down */
	Fast = 0x66,	/* same as above + regs already setup at return */
	Unimpl = 0x67,	/* syscall is not implemented yet */
	Ukn = Unimpl,	/* source compat (XXX: kill me!) */
};

/*
 * Every entry in the systen tables is described by this structure.
 */
struct sysent {
	void	*se_syscall;	/* function to call */
	short	se_nargs;	/* number of aguments */

	/*
	 * Theses are only used for syscall tracing.
	 */
	char	*se_name;	/* name of function */
	char	*se_args;	/* how to print the argument list */
};

/*
 * Types for syscall pointers.
 */
typedef int (*syscall_t)(struct pt_regs *);
typedef int (*syscallv_t)(void);
typedef int (*syscall1_t)(int);
typedef int (*syscall2_t)(int, int);
typedef int (*syscall3_t)(int, int, int);
typedef int (*syscall4_t)(int, int, int, int);
typedef int (*syscall5_t)(int, int, int, int, int);
typedef int (*syscall6_t)(int, int, int, int, int, int);
typedef int (*syscall7_t)(int, int, int, int, int, int, int);

/*
 * Marcos to call syscall pointers.
 */
#define SYSCALL_VOID(sys) \
	((syscallv_t)(sys))();
#define SYSCALL_PREGS(sys, regs) \
	((syscall_t)(sys))((regs))
#define SYSCALL_1ARG(sys, args) \
	((syscall1_t)(sys))((args)[0])
#define SYSCALL_2ARG(sys, args) \
	((syscall2_t)(sys))((args)[0], (args)[1])
#define SYSCALL_3ARG(sys, args) \
	((syscall3_t)(sys))((args)[0], (args)[1], (args)[2])
#define SYSCALL_4ARG(sys, args) \
	((syscall4_t)(sys))((args)[0], (args)[1], (args)[2], (args)[3])
#define SYSCALL_5ARG(sys, args) \
	((syscall5_t)(sys))((args)[0], (args)[1], (args)[2], \
			    (args)[3], (args)[4])
#define SYSCALL_6ARG(sys, args) \
	((syscall6_t)(sys))((args)[0], (args)[1], (args)[2], \
			    (args)[3], (args)[4], (args)[5])
#define SYSCALL_7ARG(sys, args) \
	((syscall7_t)(sys))((args)[0], (args)[1], (args)[2], \
			    (args)[3], (args)[4], (args)[5], (args)[6])

#endif /* _ABI_SYSENT_H */
