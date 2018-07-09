
abi-uw7-objs	:= access.o context.o ioctl.o lfs.o mac.o \
		   misc.o mmap.o stat.o sysent.o

obj-$(CONFIG_ABI_UW7)	+= abi-uw7.o

abi-uw7.o: $(abi-uw7-objs)
	$(LD) -r -o $@ $(abi-uw7-objs)
