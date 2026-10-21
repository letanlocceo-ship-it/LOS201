#!/bin/sh
echo "=== STATS TRUOC ==="; cat /proc/sms_stats
echo "=== RUN 500ms/60s (FIFO) + snapshot RSS ==="
./sms_app -i 500 -d 60 -s 1 2>/dev/null > /tmp/run500.txt &
PID=$!
sleep 5;  echo "--- T=5s ---";  grep -E 'VmSize|VmRSS' /proc/$PID/status
sleep 25; echo "--- T=30s ---"; grep -E 'VmSize|VmRSS' /proc/$PID/status; cat /proc/$PID/maps
sleep 25; echo "--- T=55s ---"; grep -E 'VmSize|VmRSS' /proc/$PID/status
wait $PID
cat /tmp/run500.txt
echo "=== STATS SAU ==="; cat /proc/sms_stats
echo "=== JITTER: 3 vong OTHER/FIFO xen ke, co 2 busy-loop nen ==="
while :; do :; done & B1=$!
while :; do :; done & B2=$!
for i in 1 2 3; do
  for s in 0 1; do
    echo "##### RUN $i sched=$s #####"
    ./sms_app -i 50 -d 55 -s $s 2>/dev/null | grep -A5 "JITTER REPORT\|HISTOGRAM\|Overflows\|Dropped"
  done
done
kill $B1 $B2
echo "=== DONE ==="
