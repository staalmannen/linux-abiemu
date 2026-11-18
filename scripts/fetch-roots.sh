#!/bin/sh
# SPDX-License-Identifier: GPL-2.0
#
# Fetch foreign root trees for the test suite (or to play with).
#
#   scripts/fetch-roots.sh [destdir]     (default: tests/roots)
#
# FreeBSD: the official freebsd/freebsd-runtime OCI image.
# NetBSD:  the root tree inside the madworx/netbsd image (a NetBSD
#          system packaged for QEMU); a slim subset is kept.
set -e
TOP=$(cd "$(dirname "$0")/.." && pwd)
DEST=${1:-$TOP/tests/roots}
FETCH=$TOP/scripts/fetch-oci-rootfs.py
mkdir -p "$DEST"

if [ ! -d "$DEST/freebsd/bin" ]; then
	echo "== FreeBSD 14.3"
	python3 "$FETCH" freebsd/freebsd-runtime 14.3 amd64 "$DEST/freebsd"
fi

if [ ! -d "$DEST/netbsd/bin" ]; then
	echo "== NetBSD 10.1"
	TMP=$(mktemp -d)
	python3 "$FETCH" --layer-min 50000000 madworx/netbsd 10.1 amd64 "$TMP"
	N=$TMP/bsd
	T=$DEST/netbsd
	mkdir -p $T/usr/bin $T/usr/lib $T/dev $T/tmp
	cp -a $N/bin $N/sbin $N/lib $N/libexec $N/etc $N/root $T/
	cp -a $N/usr/libexec $T/usr/
	cp -a $N/usr/lib/*.so* $T/usr/lib/
	for b in id head wc tr uname env sort uniq sed awk grep find xargs \
		 less more vi tail touch true false basename dirname; do
		[ -e $N/usr/bin/$b ] && cp -a $N/usr/bin/$b $T/usr/bin/
	done
	chmod 1777 $T/tmp
	rm -rf "$TMP"
fi
