
abi-isc-objs	:= sysent.o

obj-$(CONFIG_ABI_ISC)		+= abi-isc.o

abi-isc.o: $(abi-isc-objs)
	$(LD) -r -o $@ $(abi-isc-objs)
