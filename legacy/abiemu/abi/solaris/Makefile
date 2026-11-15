
abi-solaris-objs:= lfs.o solarisx86.o socket.o stat.o sysent.o

obj-$(CONFIG_ABI_SOLARIS)	+= abi-solaris.o

abi-solaris.o: $(abi-solaris-objs)
	$(LD) -r -o $@ $(abi-solaris-objs)
