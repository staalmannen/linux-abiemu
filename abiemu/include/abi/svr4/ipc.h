/*
 * Copyright (c) 1994 Mike Jagdis.
 * Copyright (c) 2001 Christoph Hellwig.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _ABI_SVR4_IPC_H
#define _ABI_SVR4_IPC_H

#ident "%W% %G%"

/*
 * General IPC
 */
enum {
	SVR4_IPC_RMID =		0,
	SVR4_IPC_SET =		1,
	SVR4_IPC_STAT =		2,
	SVR4_IPC_RMID_L =	10,
	SVR4_IPC_SET_L =	11,
	SVR4_IPC_STAT_L =	12,
};

struct ibcs2_ipc_perm {
	u_int16_t		uid;	/* owner's user id */
	u_int16_t		gid;	/* owner's group id */
	u_int16_t		cuid;	/* creator's user id */
	u_int16_t		cgid;	/* creator's group id */
	u_int16_t		mode;	/* access modes */
	u_int16_t		seq;	/* slot usage sequence number */
	int32_t			key;	/* key */
};

struct abi4_ipc_perm {
	u_int32_t		uid;	/* owner's user id */
	u_int32_t		gid;	/* owner's group id */
	u_int32_t		cuid;	/* creator's user id */
	u_int32_t		cgid;	/* creator's group id */
	u_int32_t		mode;	/* access modes */
	u_int32_t		seq;	/* slot usage sequence number */
	int32_t			key;	/* key */
	int32_t			pad[4];	/* reserved */
};

/*
 * Message queues
 */
enum {
	SVR4_msgget =	0,
	SVR4_msgctl =	1,
	SVR4_msgrcv =	2,
	SVR4_msgsnd =	3,
};

struct ibcs2_msqid_ds {
	struct ibcs2_ipc_perm	msg_perm;
	struct msg		*msg_first;
	struct msg		*msg_last;
	u_int16_t		msg_cbytes;
	u_int16_t		msg_qnum;
	u_int16_t		msg_qbytes;
	u_int16_t		msg_lspid;
	u_int16_t		msg_lrpid;
	time_t			msg_stime;
	time_t			msg_rtime;
	time_t			msg_ctime;
};

struct abi4_msg {
	struct abi4_msg		*msg_next;
	int32_t			msg_type;
	u_int16_t		msg_ts;
	int16_t			msg_spot;
};

struct abi4_msqid_ds {
	struct abi4_ipc_perm	msg_perm;
	struct msg		*msg_first;
	struct msg		*msg_last;
	u_int32_t		msg_cbytes;
	u_int32_t		msg_qnum;
	u_int32_t		msg_qbytes;
	u_int32_t		msg_lspid;
	u_int32_t		msg_lrpid;
	u_int32_t		msg_stime;
	u_int32_t		msg_pad1;
	u_int32_t		msg_rtime;
	u_int32_t		msg_pad2;
	u_int32_t		msg_ctime;
	u_int32_t		msg_pad3;
	u_int32_t		msg_pad4[4];
};

/*
 * Shared memory
 */
enum {
	SVR4_shmat =	0,
	SVR4_shmctl =	1,
	SVR4_shmdt =	2,
	SVR4_shmget =	3,
};

/* shmctl() operations */
enum {
	SVR4_SHM_LOCK =		3,
	SVR4_SHM_UNLOCK =	4,
};

struct ibcs2_shmid_ds {
	struct ibcs2_ipc_perm	shm_perm;	/* operation permissions */
	int32_t			shm_segsz;	/* size of segment in bytes */
	struct region		*__pad1;	/* ptr to region structure */
	char			__pad2[4];	/* for swap compatibility */
	u_int16_t		shm_lpid;	/* pid of last shmop */
	u_int16_t		shm_cpid;	/* pid of creator */
	u_int16_t		shm_nattch;	/* used only for shminfo */
	u_int16_t		 __pad3;
	time_t			shm_atime;	/* last shmat time */
	time_t			shm_dtime;	/* last shmdt time */
	time_t			shm_ctime;	/* last change time */
};

struct abi4_shmid_ds {
	struct abi4_ipc_perm	shm_perm;	/* operation permissions */
	int32_t			shm_segsz;	/* size of segment in bytes */
	struct region		*__pad1;	/* ptr to region structure */
	u_int16_t		shm_lckcnt;	/* number of locks */
	char			__pad2[2];	/* for swap compatibility */
	u_int32_t		shm_lpid;	/* pid of last shmop */
	u_int32_t		shm_cpid;	/* pid of creator */
	u_int32_t		shm_nattch;/* used only for shminfo */
	u_int32_t		shm_cnattch;
	u_int32_t		shm_atime;	/* last shmat time */
	u_int32_t		shm_pad1;
	u_int32_t		shm_dtime;	/* last shmdt time */
	u_int32_t		shm_pad2;
	u_int32_t		shm_ctime;	/* last change time */
	u_int32_t		shm_pad3;
	u_int32_t		shm_pad4[4];
};

/*
 * Semaphores
 */
enum {
	SVR4_semctl =	0,
	SVR4_semget =	1,
	SVR4_semop =	2,
};

/* semctl() operations */
enum {
	SVR4_SEM_GETNCNT =	3,
	SVR4_SEM_GETPID =	4,
	SVR4_SEM_GETVAL	=	5,
	SVR4_SEM_GETALL	=	6,
	SVR4_SEM_GETZCNT =	7,
	SVR4_SEM_SETVAL =	8,
	SVR4_SEM_SETALL =	9,
};

/* mapping of svr4 semaphore operations to Linux (if available) */
static int svr4sem2linux[] = {
	[SVR4_IPC_RMID]	=	IPC_RMID,
	[SVR4_IPC_SET] =	IPC_SET,
	[SVR4_IPC_STAT] =	IPC_STAT,
	[SVR4_SEM_GETNCNT] =	GETNCNT,
	[SVR4_SEM_GETPID] =	GETPID,
	[SVR4_SEM_GETVAL] =	GETVAL,
	[SVR4_SEM_GETALL] =	GETALL,
	[SVR4_SEM_GETZCNT] =	GETZCNT,
	[SVR4_SEM_SETVAL] =	SETVAL,
	[SVR4_SEM_SETALL] =	SETALL,
	[SVR4_IPC_RMID_L] =	IPC_RMID,
	[SVR4_IPC_SET_L] =	IPC_SET,
	[SVR4_IPC_STAT_L] =	IPC_STAT,
};

struct ibcs2_semid_ds {
	struct ibcs2_ipc_perm	sem_perm;
	struct sem		*sem_base;
	u_int16_t		sem_nsems;
	char			__pad[2];
	u_int32_t		sem_otime;
	u_int32_t		sem_ctime;
};

struct abi4_semid_ds {
	struct abi4_ipc_perm	sem_perm;
	struct sem		*sem_base;
	u_int16_t		sem_nsems;
	char			__pad[2]; /* this pad is not in the abi doc! */
	u_int32_t		sem_otime;
	u_int32_t		sem_pad1;
	u_int32_t		sem_ctime;
	u_int32_t		sem_pad2;
	u_int32_t		sem_pad3[4];
};

#endif /* _ABI_SVR4_IPC_H */
