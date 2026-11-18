# Runs inside the QEMU guest (see run-qemu.sh).  Expects $OS and $MODS.
cd /mod || exit 1
for m in $MODS; do
	insmod $m.ko $( [ $m = abiemu ] && echo trace=1 ) || { echo "RESULT $OS: FAIL (insmod $m)"; exit 1; }
done
R=/roots/$OS
mkdir -p $R/dev $R/tmp
mount -t devtmpfs dev $R/dev
cp /t/workload.sh $R/tmp/
chroot $R /bin/sh /tmp/workload.sh > /tmp/out.txt 2>&1
cat /tmp/out.txt
fail=0
while IFS= read -r line; do
	grep -qxF -- "$line" /tmp/out.txt || { echo "MISSING: $line"; fail=1; }
done < /t/expected.txt
dmesg | grep -E "abiemu: $OS|BUG|Oops|WARNING|bad signal frame" | grep -v cap_enter
[ $fail = 0 ] && echo "RESULT $OS: PASS" || echo "RESULT $OS: FAIL"
