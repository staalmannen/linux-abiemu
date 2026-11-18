echo "== pipes and redirects"
echo one two three | cat | cat > /tmp/a.txt; cat /tmp/a.txt
ls /bin | cat | { read x; echo "first: $x"; }
echo "== files"
seq_file=/etc/passwd; mkdir -p /tmp/d/e && cp $seq_file /tmp/d/c && ln -s c /tmp/d/l && ln /tmp/d/c /tmp/d/h && ls -l /tmp/d
chmod 600 /tmp/d/c; mv /tmp/d/c /tmp/d/c2; rm /tmp/d/l; echo $(ls /tmp/d); cd /tmp/d/../d && pwd -P; cd /
dd if=/etc/passwd of=/tmp/dd.out bs=512 count=2 2>&1; ls -l /tmp/dd.out
rm -r /tmp/d; test -e /tmp/d || echo "removed"
echo "== traps"
trap 'echo got USR1' USR1; kill -USR1 $$; echo after-usr1
trap 'echo got TERM' TERM; kill -TERM $$; echo after-term
echo "== background and wait"
(sleep 1; echo child done; exit 7) & wait $!; echo "wait status $?"
sleep 5 & sleep 1; kill $!; wait $!; echo "killed status $? (expect 143)"
echo "== misc"
expr 3 + 4; hostname; date +%Y; test -d /bin && echo isdir; echo $$ > /dev/null; pwd; cd /usr && pwd
df / 2>&1 | head -3
stty -a < /dev/console 2>&1 | head -2
kenv 2>&1 | head -1; nproc; uuidgen | wc -c 2>&1; id 2>&1; ls /usr/bin | head -20 | tr '\n' ' '
echo; echo "== here-doc"
cat <<X
heredoc $((1+1))
X
echo "== done"
