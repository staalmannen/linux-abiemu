
abi-wyse-objs	:= sysent.o ptrace.o socket.o

obj-$(CONFIG_ABI_WYSE)		+= abi-wyse.o

abi-wyse.o: $(abi-wyse-objs)
	$(LD) -r -o $@ $(abi-wyse-objs)
