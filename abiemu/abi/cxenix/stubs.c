/*
 * stubs.c - stubs for unimplemented cxenix subcalls
 *
 * Copyright (c) 1993,1994 Drew Sullivan.
 * Copyright (c) 1994-1996 Mike Jagdis.
 */

#ident "%W% %G%"

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/ptrace.h>

#include <abi/cxenix/sysent.h>


int
xnx_creatsem(char *sem_name, int mode)
{
	return -EPERM;
}

int
xnx_opensem(char *sem_name)
{
	return -EPERM;
}

int
xnx_sigsem(int sem_num)
{
	return -EPERM;
}

int
xnx_waitsem(int sem_num)
{
	return -EPERM;
}

int
xnx_nbwaitsem(int sem_num)
{
	return -EPERM;
}


int
xnx_sdget(char *path, int flags, long size, int mode)
{
	return -EPERM;
}

int
xnx_sdfree(char* addr)
{
	return -EPERM;
}

int
xnx_sdenter(char *addr, int flags)
{
	return -EPERM;
}

int
xnx_sdleave(char *addr)
{
	return -EPERM;
}

int
xnx_sdgetv(char *addr)
{
	return -EPERM;
}

int
xnx_sdwaitv(char *addr, int vnum)
{
	return -EPERM;
}


/*
 * This allows processes to be allowed to exceed available swap. The man
 * page isn't too clear  - it seems to suggest Xenix supports physical
 * memory > swap but this doesn't make sense to me? It almost certainly
 * isn't useful for Linux to actually do anything with this - just lie.
 */
enum {
	XNX_PRHUGEX =	1,
	XNX_PRNORMEX =	2,
};

int
xnx_proctl(int pid, int command, char *arg)
{
	return 0;
}

int
xnx_execseg(excode_t oldaddr, unsigned size)
{
	return -EPERM;
}


int
xnx_unexecseg(excode_t addr)
{
	return -EPERM;
}

/*
 * This allows running adb without executing any programs, but disassembly
 * will work fine with that lie.
 */
int
xnx_paccess(int pid, int cmd, int offset, int count, char *ptr)
{
	return 0;
}
