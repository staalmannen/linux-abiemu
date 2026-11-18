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

run: run-rawsys $(SUITES:%=run-%)

# Freestanding raw system call programs, one per OS; no roots needed.
run-rawsys:
	@$(MAKE) -s -C rawsys
	@./run-qemu.sh rawsys/guest.sh rawsys/rawsys-* | grep -a -E '^(RESULT|not ok)'

run-%:
	@if [ -d roots/$*/bin ]; then \
		./run-qemu.sh -r $*=roots/$* $*/guest.sh guest-common.sh \
			workload.sh expected.txt | grep -a -E '^(RESULT|MISSING|\[)'; \
	else echo "RESULT $*: SKIP (no roots/$*)"; fi

.PHONY: run run-rawsys
