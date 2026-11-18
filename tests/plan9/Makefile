# SPDX-License-Identifier: GPL-2.0
# Plan 9 a.out test programs, built with gcc and converted by elf2aout.py
CC ?= gcc
CFLAGS = -O1 -ffreestanding -fno-stack-protector -fno-pie -fno-builtin \
	 -fcf-protection=none -mno-red-zone -fno-asynchronous-unwind-tables \
	 -mgeneral-regs-only -Wall -Wno-unused-variable
LDFLAGS = -nostdlib -static -no-pie -Wl,--build-id=none \
	  -Wl,-T,../../scripts/plan9-aout.ld

PROGS = p9test
all: $(PROGS)

p9test.elf: p9test.c sys.S
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ p9test.c sys.S

p9test: p9test.elf
	python3 ../../scripts/elf2aout.py $< $@
	chmod +x $@

clean:
	rm -f $(PROGS) *.elf
