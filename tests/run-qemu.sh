#!/bin/sh
# SPDX-License-Identifier: GPL-2.0
#
# Boot a kernel in QEMU with the abiemu modules and run a script.
#
#   run-qemu.sh [-r name=dir]... [-t seconds] <guest-script> [files...]
#
# The guest is a busybox initramfs.  The modules are in /mod, extra
# files in /t, and each "-r name=dir" tree is copied to /roots/name
# (e.g. an unpacked FreeBSD base system to chroot into).
#
# Environment:
#   KVER    kernel version to boot, needs /boot/vmlinuz-$KVER
#   KERNEL  kernel image (default /boot/vmlinuz-$KVER)
#   BUSYBOX static busybox binary (default: from $PATH)
#   QEMU_OPTS  extra QEMU options, e.g. "-enable-kvm"
set -e

KVER=${KVER:-$(uname -r)}
KERNEL=${KERNEL:-/boot/vmlinuz-$KVER}
BUSYBOX=${BUSYBOX:-$(command -v busybox)}
TOP=$(cd "$(dirname "$0")/.." && pwd)
TIMEOUT=300
ROOTS=

while [ $# -gt 0 ]; do
	case "$1" in
	-r) ROOTS="$ROOTS $2"; shift 2 ;;
	-t) TIMEOUT=$2; shift 2 ;;
	*) break ;;
	esac
done
SCRIPT=$1; shift

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
R=$WORK/root
mkdir -p $R/bin $R/mod $R/t $R/proc $R/sys $R/dev $R/tmp $R/roots

cp "$BUSYBOX" $R/bin/busybox
for m in "$TOP"/*/*.ko; do cp "$m" $R/mod/; done
cp "$SCRIPT" $R/t/run.sh
for f in "$@"; do cp -a "$f" $R/t/; done
for r in $ROOTS; do
	mkdir -p "$R/roots/${r%%=*}"
	cp -a "${r#*=}/." "$R/roots/${r%%=*}/"
done

cat > $R/init <<'INIT'
#!/bin/busybox sh
/bin/busybox --install -s /bin
mount -t proc proc /proc
mount -t sysfs sys /sys
mount -t devtmpfs dev /dev
echo "=== guest $(uname -r) ==="
sh /t/run.sh
echo "=== guest exit status $? ==="
poweroff -f
INIT
chmod +x $R/init

(cd $R && find . | cpio -o -H newc --quiet | gzip -1) > $WORK/initrd.gz

timeout "$TIMEOUT" qemu-system-x86_64 -cpu max -m 1024 -smp 2 \
	-kernel "$KERNEL" -initrd $WORK/initrd.gz \
	-append "console=ttyS0 panic=-1 loglevel=6 quiet rdinit=/init" \
	-nographic -no-reboot $QEMU_OPTS < /dev/null
