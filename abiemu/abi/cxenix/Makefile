
abi-cxenix-objs	:= sysent.o misc.o stubs.o signal.o pathconf.o utsname.o

obj-$(CONFIG_ABI_SCO)	+= abi-cxenix.o

abi-cxenix.o: $(abi-cxenix-objs)
	$(LD) -r -o $@ $(abi-cxenix-objs)
