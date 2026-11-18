# Runs inside the QEMU guest (see tests/run-qemu.sh)
cd /mod
insmod abiemu.ko trace=1 && insmod abi_bsd.ko && insmod abi_freebsd.ko || exit 1
F=/roots/freebsd
mount -t devtmpfs dev $F/dev
cp /t/workload.sh $F/tmp/
chroot $F /bin/sh /tmp/workload.sh > /tmp/out.txt 2>&1
cat /tmp/out.txt
fail=0
while IFS= read -r line; do
	grep -qxF -- "$line" /tmp/out.txt || { echo "MISSING: $line"; fail=1; }
done < /t/expected.txt
dmesg | grep -E "abiemu: freebsd|BUG|Oops|WARNING" | grep -v cap_enter
[ $fail = 0 ] && echo "RESULT freebsd: PASS" || echo "RESULT freebsd: FAIL"
