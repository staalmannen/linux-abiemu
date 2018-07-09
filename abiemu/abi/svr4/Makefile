
svr4-y		:= hrtsys.o ioctl.o ipc.o mmap.o open.o svr4.o sysconf.o \
		   sysfs.o sysinfo.o sysi86.o ulimit.o utsname.o stream.o \
		   stat.o socksys.o signal.o misc.o socket.o statvfs.o \
		   fcntl.o

# emulations for all kinds of weird ioctls...
svr4-y		+= filio.o termios.o consio.o tapeio.o sockio.o

# networking
svr4-$(CONFIG_ABI_XTI)	+= timod.o xti.o
svr4-$(CONFIG_ABI_SPX)	+= timod.o

abi-svr4-objs	+= $(sort $(svr4-y))


obj-$(CONFIG_ABI_SVR4)	+= abi-svr4.o

abi-svr4.o: $(abi-svr4-objs)
	$(LD) -r -o $@ $(abi-svr4-objs)
