#!/bin/sh

MODULE=/lib/modules/5.15.0/counter_driver.ko
DEV=/dev/counter
PROC=/proc/counter_info

PASS=0
FAIL=0

ok()
{
    echo "[PASS] $1"
    PASS=$((PASS + 1))
}

bad()
{
    echo "[FAIL] $1"
    FAIL=$((FAIL + 1))
}

echo "========================================"
echo " ASSIGNMENT 1 - TASK 2 COUNTER TEST"
echo "========================================"

if lsmod | grep -q '^counter_driver '; then
    rmmod counter_driver
fi

echo
echo "===== 1. LOAD MODULE ====="

if insmod "$MODULE"; then
    ok "counter_driver loaded"
else
    bad "cannot load counter_driver"
    exit 1
fi

echo
echo "===== 2. DEVICE CHECK ====="

lsmod | grep counter_driver
cat /proc/misc | grep counter
ls -l "$DEV"

if [ -e "$DEV" ]; then
    ok "/dev/counter exists"
else
    bad "/dev/counter does not exist"
fi

if [ -e "$PROC" ]; then
    ok "/proc/counter_info exists"
else
    bad "/proc/counter_info does not exist"
fi

echo
echo "===== 3. PROC BEFORE WRITE ====="

PROC1=$(cat "$PROC")
echo "$PROC1"

if echo "$PROC1" | grep -q 'Current counter: 0'; then
    ok "initial counter = 0"
else
    bad "initial counter is not 0"
fi

echo
echo "===== 4. WRITE 100 ====="

echo 100 > "$DEV"

PROC2=$(cat "$PROC")
echo "$PROC2"

if echo "$PROC2" | grep -q 'Current counter: 100'; then
    ok "write 100 sets counter to 100"
else
    bad "counter is not 100"
fi

echo
echo "===== 5. RESET COUNTER ====="

echo 0 > "$DEV"

PROC3=$(cat "$PROC")
echo "$PROC3"

if echo "$PROC3" | grep -q 'Current counter: 0'; then
    ok "write 0 resets counter"
else
    bad "counter did not reset"
fi

echo
echo "===== 6. AUTO-INCREMENT READ ====="

R1=$(cat "$DEV")
R2=$(cat "$DEV")
R3=$(cat "$DEV")

echo "Read #1 = $R1"
echo "Read #2 = $R2"
echo "Read #3 = $R3"

if [ "$R1" = "0" ] && [ "$R2" = "1" ] && [ "$R3" = "2" ]; then
    ok "read sequence is 0, 1, 2"
else
    bad "unexpected read sequence"
fi

echo
echo "===== 7. SET 100 AND READ ====="

echo 100 > "$DEV"
R100=$(cat "$DEV")

echo "Read result = $R100"

if [ "$R100" = "100" ]; then
    ok "set/read 100"
else
    bad "expected 100"
fi

echo
echo "===== 8. RESET 0 AND READ ====="

echo 0 > "$DEV"
R0=$(cat "$DEV")

echo "Read result = $R0"

if [ "$R0" = "0" ]; then
    ok "reset/read 0"
else
    bad "expected 0"
fi

echo
echo "===== 9. INVALID INPUT ====="

ERR1=/tmp/counter_neg_error
ERR2=/tmp/counter_text_error

if echo -5 > "$DEV" 2>"$ERR1"; then
    bad "negative value was accepted"
else
    ok "negative value rejected with error"
    cat "$ERR1"
fi

if echo abc > "$DEV" 2>"$ERR2"; then
    bad "non-numeric value was accepted"
else
    ok "non-numeric value rejected with error"
    cat "$ERR2"
fi

rm -f "$ERR1" "$ERR2"

echo
echo "===== 10. FINAL PROC INFO ====="
cat "$PROC"

echo
echo "===== 11. UNLOAD MODULE ====="

if rmmod counter_driver; then
    ok "counter_driver unloaded"
else
    bad "cannot unload counter_driver"
fi

if lsmod | grep -q '^counter_driver '; then
    bad "counter_driver still present"
else
    ok "counter_driver removed from lsmod"
fi

if [ -e "$DEV" ]; then
    bad "/dev/counter still exists"
else
    ok "/dev/counter removed"
fi

if [ -e "$PROC" ]; then
    bad "/proc/counter_info still exists"
else
    ok "/proc/counter_info removed"
fi

echo
echo "===== 12. DMESG ====="
dmesg | grep counter_driver | tail -40

echo
echo "========================================"
echo "PASS = $PASS"
echo "FAIL = $FAIL"
echo "========================================"

if [ "$FAIL" -eq 0 ]; then
    echo "TASK 2 RESULT: PASS"
    exit 0
else
    echo "TASK 2 RESULT: FAIL"
    exit 1
fi
