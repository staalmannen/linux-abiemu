# SPDX-License-Identifier: GPL-2.0
# Build rawsys for every target OS; the result is branded the way the
# real system brands its binaries.
CC ?= gcc
CFLAGS = -O1 -ffreestanding -fno-stack-protector -fno-pie -fno-builtin \
	 -fcf-protection=none -Wall
LDFLAGS = -nostdlib -static -no-pie -Wl,--build-id=none

OSES = freebsd netbsd openbsd illumos
all: $(OSES:%=rawsys-%)

rawsys-%: rawsys.c notes.S
	$(CC) $(CFLAGS) $(LDFLAGS) -DOS_$(shell echo $* | tr a-z A-Z) -o $@ rawsys.c notes.S
	@case $* in \
	freebsd) printf '\011' | dd of=$@ bs=1 seek=7 conv=notrunc status=none ;; \
	illumos) printf '\006' | dd of=$@ bs=1 seek=7 conv=notrunc status=none ;; \
	esac

clean:
	rm -f $(OSES:%=rawsys-%)
