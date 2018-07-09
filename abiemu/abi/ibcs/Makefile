
abi-ibcs-objs	:= sysent.o

obj-$(CONFIG_ABI_IBCS)		+= abi-ibcs.o

abi-ibcs.o: $(abi-ibcs-objs)
	$(LD) -r -o $@ $(abi-ibcs-objs)
