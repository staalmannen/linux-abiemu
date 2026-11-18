# SPDX-License-Identifier: GPL-2.0
#
# make -C tests run KVER=<kernel>      run every suite whose root exists
#
# Suites:
#   freebsd  needs a FreeBSD base in tests/roots/freebsd:
#            ../scripts/fetch-oci-rootfs.py freebsd/freebsd-runtime 14.3 amd64 roots/freebsd
#   rawsys   self contained raw system call programs (NetBSD, OpenBSD,
#            illumos, Plan 9), built here
KVER ?= $(shell uname -r)
export KVER

run:
	@if [ -d roots/freebsd/bin ]; then \
		./run-qemu.sh -r freebsd=roots/freebsd freebsd/guest.sh \
			freebsd/workload.sh freebsd/expected.txt | grep -a -A200 '=== guest'; \
	else echo "skipping freebsd suite: no roots/freebsd"; fi

.PHONY: run rawsys
