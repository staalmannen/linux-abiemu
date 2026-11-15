# SPDX-License-Identifier: GPL-2.0
#
# Out of tree build of the abiemu kernel modules.
#
#   make                  build against the running kernel
#   make KVER=6.17.0-42-generic
#   make KDIR=/path/to/kernel/build
#   make test             boot a kernel in QEMU and run the test suite
#
KVER ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build

all: modules

modules clean modules_install:
	$(MAKE) -C $(KDIR) M=$(CURDIR) $@

test: modules
	$(MAKE) -C tests KVER=$(KVER) run

.PHONY: all modules clean modules_install test
