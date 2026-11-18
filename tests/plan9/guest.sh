# Runs inside the QEMU guest
cd /mod || exit 1
echo 1 > /proc/sys/debug/exception-trace
ip link set lo up 2>/dev/null || ifconfig lo 127.0.0.1 up
insmod abiemu.ko trace=1 && insmod abi_plan9.ko || exit 1
insmod abi_p9net.ko && mkdir -p /net && mount -t p9net none /net
for p in /t/p9*; do
	n=${p##*/}
	cp $p /bin/$n; chmod +x /bin/$n
	/bin/$n > /tmp/$n.out 2>&1
	rc=$?
	cat /tmp/$n.out
	if [ $rc = 0 ] && grep -q "# all passed" /tmp/$n.out; then
		echo "RESULT plan9-$n: PASS"
	else
		echo "RESULT plan9-$n: FAIL (exit $rc)"
	fi
done
dmesg | grep -E "abiemu: plan9|p9net|traps:|BUG|Oops|WARNING"
