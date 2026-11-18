# Runs inside the QEMU guest: every rawsys-<os> binary under its personality
cd /mod || exit 1
insmod abiemu.ko trace=1 && insmod abi_bsd.ko || exit 1
for m in abi_*.ko; do [ $m = abi_bsd.ko ] || insmod $m; done
for t in /t/rawsys-*; do
	os=${t##*-}
	$t > /tmp/$os.out 2>&1
	rc=$?
	cat /tmp/$os.out
	if [ $rc = 0 ] && grep -q "# all passed" /tmp/$os.out; then
		echo "RESULT rawsys-$os: PASS"
	else
		echo "RESULT rawsys-$os: FAIL (exit $rc)"
	fi
done
dmesg | grep -E "abiemu: (freebsd|netbsd|openbsd|illumos|plan9)|BUG|Oops|WARNING"
