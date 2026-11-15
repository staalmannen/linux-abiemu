/*
 * Exports all Linux syscalls.
 * Christoph Hellwig (hch@caldera.de), 2001
 */

#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>
#include <linux/syscalls.h>

#include <linux/config.h>
#include <linux/module.h>
#include <linux/personality.h>
#include <linux/msg.h>
#include <linux/sem.h>
#include <linux/shm.h>
#include <linux/fs.h>

EXPORT_SYMBOL(sys_uselib);
EXPORT_SYMBOL(sys_sync);
EXPORT_SYMBOL(sys_fsync);
EXPORT_SYMBOL(sys_fdatasync);
EXPORT_SYMBOL(sys_dup);
EXPORT_SYMBOL(sys_dup2);
EXPORT_SYMBOL(sys_fcntl);
#if (BITS_PER_LONG == 32)
EXPORT_SYMBOL(sys_fcntl64);
#endif
EXPORT_SYMBOL(sys_ioctl);
EXPORT_SYMBOL(sys_link);
EXPORT_SYMBOL(sys_mkdir);
EXPORT_SYMBOL(sys_mknod);
EXPORT_SYMBOL(sys_rename);
EXPORT_SYMBOL(sys_rmdir);
EXPORT_SYMBOL(sys_symlink);
EXPORT_SYMBOL(sys_unlink);
EXPORT_SYMBOL(sys_umount);
EXPORT_SYMBOL(sys_oldumount);
EXPORT_SYMBOL(sys_mount);
EXPORT_SYMBOL(sys_pivot_root);
EXPORT_SYMBOL(sys_access);
EXPORT_SYMBOL(sys_chdir);
EXPORT_SYMBOL(sys_fchdir);
EXPORT_SYMBOL(sys_chroot);
EXPORT_SYMBOL(sys_chmod);
EXPORT_SYMBOL(sys_fchmod);
EXPORT_SYMBOL(sys_chown);
EXPORT_SYMBOL(sys_lchown);
EXPORT_SYMBOL(sys_fchown);
EXPORT_SYMBOL(sys_close);
#if !defined(__alpha__)
EXPORT_SYMBOL(sys_creat);
#endif
EXPORT_SYMBOL(sys_open);
EXPORT_SYMBOL(sys_statfs);
EXPORT_SYMBOL(sys_fstatfs);
EXPORT_SYMBOL(sys_ftruncate);
EXPORT_SYMBOL(sys_ftruncate64);
EXPORT_SYMBOL(sys_truncate);
EXPORT_SYMBOL(sys_truncate64);
#if !defined(__alpha__) && !defined(__ia64__)
EXPORT_SYMBOL(sys_utime);
#endif
EXPORT_SYMBOL(sys_utimes);
EXPORT_SYMBOL(sys_vhangup);
#if !defined(__alpha__)
EXPORT_SYMBOL(sys_lseek);
#endif
EXPORT_SYMBOL(sys_llseek);
EXPORT_SYMBOL(sys_read);
EXPORT_SYMBOL(sys_readv);
EXPORT_SYMBOL(sys_pread64);
EXPORT_SYMBOL(sys_pwrite64);
EXPORT_SYMBOL(sys_write);
EXPORT_SYMBOL(sys_writev);
EXPORT_SYMBOL(old_readdir);
EXPORT_SYMBOL(sys_poll);
EXPORT_SYMBOL(sys_select);
EXPORT_SYMBOL(sys_readlink);
EXPORT_SYMBOL(sys_sysfs);
EXPORT_SYMBOL(sys_acct);
EXPORT_SYMBOL(sys_exit);
EXPORT_SYMBOL(sys_getitimer);
EXPORT_SYMBOL(sys_setitimer);
EXPORT_SYMBOL(sys_gettimeofday);
EXPORT_SYMBOL(sys_settimeofday);
EXPORT_SYMBOL(sys_stime);
EXPORT_SYMBOL(sys_time);
#if !defined(__alpha__)
EXPORT_SYMBOL(sys_nice);
#endif
EXPORT_SYMBOL(sys_sched_setscheduler);
EXPORT_SYMBOL(sys_sched_setparam);
EXPORT_SYMBOL(sys_sched_getscheduler);
EXPORT_SYMBOL(sys_sched_getparam);
EXPORT_SYMBOL(sys_sched_get_priority_max);
EXPORT_SYMBOL(sys_sched_get_priority_min);
EXPORT_SYMBOL(sys_sched_rr_get_interval);
EXPORT_SYMBOL(sys_kill);
EXPORT_SYMBOL(sys_rt_sigaction);
EXPORT_SYMBOL(sys_rt_sigpending);
EXPORT_SYMBOL(sys_rt_sigprocmask);
EXPORT_SYMBOL(sys_rt_sigtimedwait);
EXPORT_SYMBOL(sys_sigaltstack);
EXPORT_SYMBOL(sys_sigpending);
EXPORT_SYMBOL(sys_sigprocmask);
EXPORT_SYMBOL(sys_sigsuspend);
EXPORT_SYMBOL(sys_gethostname);
EXPORT_SYMBOL(sys_sethostname);
EXPORT_SYMBOL(sys_setdomainname);
EXPORT_SYMBOL(sys_getrlimit);
EXPORT_SYMBOL(sys_setsid);
EXPORT_SYMBOL(sys_getsid);
EXPORT_SYMBOL(sys_getpgid);
EXPORT_SYMBOL(sys_setpgid);
EXPORT_SYMBOL(sys_getgroups);
EXPORT_SYMBOL(sys_setgroups);
EXPORT_SYMBOL(sys_setpriority);
EXPORT_SYMBOL(sys_getpriority);
EXPORT_SYMBOL(sys_reboot);
EXPORT_SYMBOL(sys_setgid);
EXPORT_SYMBOL(sys_setuid);
EXPORT_SYMBOL(sys_times);
EXPORT_SYMBOL(sys_umask);
EXPORT_SYMBOL(sys_prctl);
EXPORT_SYMBOL(sys_setreuid);
EXPORT_SYMBOL(sys_setregid);
#if !defined(__alpha__) && !defined(__ia64__)
EXPORT_SYMBOL(sys_alarm);
#endif
#if !defined(__alpha__)
EXPORT_SYMBOL(sys_getpid);
EXPORT_SYMBOL(sys_getppid);
EXPORT_SYMBOL(sys_getuid);
EXPORT_SYMBOL(sys_geteuid);
EXPORT_SYMBOL(sys_getgid);
EXPORT_SYMBOL(sys_getegid);
#endif
EXPORT_SYMBOL(sys_gettid);
EXPORT_SYMBOL(sys_nanosleep);
#if defined(CONFIG_UID16)
EXPORT_SYMBOL(sys_setreuid16);
EXPORT_SYMBOL(sys_setregid16);
EXPORT_SYMBOL(sys_getgroups16);
EXPORT_SYMBOL(sys_setgroups16);
#endif
EXPORT_SYMBOL(sys_wait4);
EXPORT_SYMBOL(sys_waitpid);
EXPORT_SYMBOL(sys_sendfile);
EXPORT_SYMBOL(sys_brk);
EXPORT_SYMBOL(sys_msync);
EXPORT_SYMBOL(sys_madvise);
EXPORT_SYMBOL(sys_mincore);
EXPORT_SYMBOL(sys_munmap);
EXPORT_SYMBOL(sys_mprotect);
EXPORT_SYMBOL(sys_socket);
EXPORT_SYMBOL(sys_socketpair);
EXPORT_SYMBOL(sys_bind);
EXPORT_SYMBOL(sys_listen);
EXPORT_SYMBOL(sys_accept);
EXPORT_SYMBOL(sys_connect);
EXPORT_SYMBOL(sys_getsockname);
EXPORT_SYMBOL(sys_getpeername);
EXPORT_SYMBOL(sys_sendto);
EXPORT_SYMBOL(sys_send);
EXPORT_SYMBOL(sys_recvfrom);
EXPORT_SYMBOL(sys_setsockopt);
EXPORT_SYMBOL(sys_getsockopt);
EXPORT_SYMBOL(sys_shutdown);
EXPORT_SYMBOL(sys_sendmsg);
EXPORT_SYMBOL(sys_recvmsg);
EXPORT_SYMBOL(sys_socketcall);

EXPORT_SYMBOL(sys_msgget);
EXPORT_SYMBOL(sys_msgsnd);
EXPORT_SYMBOL(sys_msgrcv);
EXPORT_SYMBOL(sys_msgctl);
EXPORT_SYMBOL(sys_semget);
EXPORT_SYMBOL(sys_semop);
EXPORT_SYMBOL(sys_semctl);
EXPORT_SYMBOL(sys_shmget);
EXPORT_SYMBOL(do_shmat);
EXPORT_SYMBOL(sys_shmdt);
EXPORT_SYMBOL(sys_shmctl);
