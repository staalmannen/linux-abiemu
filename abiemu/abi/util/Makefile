
abi-util-objs	:= stat.o plist.o

obj-$(CONFIG_ABI_UTIL)	+= abi-util.o

abi-util.o: $(abi-util-objs)
	$(LD) -r -o $@ $(abi-util-objs)
