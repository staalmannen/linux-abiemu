# Runs inside the QEMU guest
cd /mod || exit 1
echo 1 > /proc/sys/debug/exception-trace
insmod abiemu.ko trace=1 && insmod abi_plan9.ko || exit 1
cp /t/p9test /bin/p9test; chmod +x /bin/p9test
/bin/p9test > /tmp/p9.out 2>&1
rc=$?
cat /tmp/p9.out
if [ $rc = 0 ] && grep -q "# all passed" /tmp/p9.out; then
	echo "RESULT plan9: PASS"
else
	echo "RESULT plan9: FAIL (exit $rc)"
fi
dmesg | grep -E "abiemu: plan9|traps:|BUG|Oops|WARNING"
