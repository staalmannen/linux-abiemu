=== Linux abi-emu project ===
This is a revival of the old linux-abi [1] project, which in turn was a maintenance 
of iBCS. Conceptually, this project is a syscall emulation layer similar to the 
Linux emulation that can be found in FreeBSD [2] or NetBSD [3]. 
A nice overview on BSD syscalls can be seen on this reddit post [4]

=== Building ===
To build abiemu as loadable kernel modules without having to rebuild the kernel, 
follow these instructions [5]:

=== AIMS / TODO ===
* Get abiemu to build as kernel modules for a current kernel
* Practice by porting the glendix [6] Plan9 syscall emulation to abiemu
* Implement NetBSD syscall emulation, try to run NetBSD in chroot
* Implement/update Solaris syscall emulation, try to run illumos in chroot
* Try to implement FreeBSD and OpenBSD syscall emulation
* Try to implement syscall emulations for more architectures
* Release and maintain proof-of-concept native self-hosting and binary-compatible 
   distros where the BSD/Solaris system is chrooted from a minimal Linux system 
   during boot. (for example:  "NetBSD/kLinux").

=== Upstream sources ===
Linux-abi svn sources were downloaded 2018-07-02 from sourceforge.
Glendix git sources were downloaded 2018-07-02 from github

===References===
1 : http://linux-abi.sourceforge.net/
2 : https://svnweb.freebsd.org/base/head/sys/i386/linux/syscalls.master?view=log
3 : http://cvsweb.netbsd.org/bsdweb.cgi/src/sys/compat/linux/arch/i386/syscalls.master?only_with_tag=MAIN
4 : https://www.reddit.com/r/BSD/comments/8vysxg/a_question_about_bsd_kernel_syscallsabi/e1vj277/
5 : https://github.com/torvalds/linux/blob/master/Documentation/kbuild/modules.txt
6 : https://github.com/anantn/glendix
