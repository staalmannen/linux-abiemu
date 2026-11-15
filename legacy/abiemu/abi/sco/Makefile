
abi-sco-objs	:= sysent.o misc.o mmap.o ptrace.o secureware.o \
		   stat.o statvfs.o

# various ioctl emulation stuff
abi-sco-objs	+= ioctl.o termios.o tapeio.o vtkbd.o


obj-$(CONFIG_ABI_SCO)		+= abi-sco.o

abi-sco.o: $(abi-sco-objs)
	$(LD) -r -o $@ $(abi-sco-objs)
