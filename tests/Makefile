# SPDX-License-Identifier: GPL-2.0
#
# make -C tests run KVER=<kernel>
#
# Boots the kernel in QEMU once per suite and runs workload.sh inside a
# chroot of the foreign system.  A suite is skipped when its root tree
# is missing.  Fetch the roots with ../scripts/fetch-roots.sh.
KVER ?= $(shell uname -r)
export KVER

SUITES = freebsd netbsd

run: run-rawsys run-plan9 $(SUITES:%=run-%)

# Freestanding raw system call programs, one per OS; no roots needed.
run-rawsys:
	@$(MAKE) -s -C rawsys
	@./run-qemu.sh rawsys/guest.sh rawsys/rawsys-* | grep -a -E '^(RESULT|not ok)'

run-%:
	@if [ -d roots/$*/bin ]; then \
		./run-qemu.sh -r $*=roots/$* $*/guest.sh guest-common.sh \
			workload.sh expected.txt | grep -a -E '^(RESULT|MISSING|\[)'; \
	else echo "RESULT $*: SKIP (no roots/$*)"; fi

# Plan 9 a.out programs built with gcc + elf2aout.py
run-plan9:
	@$(MAKE) -s -C plan9
	@./run-qemu.sh plan9/guest.sh plan9/p9test plan9/p9net | grep -a -E '^(RESULT|not ok)'

.PHONY: run run-rawsys run-plan9
