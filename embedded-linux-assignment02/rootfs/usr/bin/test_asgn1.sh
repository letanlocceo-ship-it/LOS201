#!/bin/sh

MODULE=/lib/modules/5.15.0/asgn1_driver.ko
DEV=/dev/asgn1
TEST=/usr/bin/test_ioctl

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
echo " ASSIGNMENT 1 - TASK 1 IOCTL TEST"
echo "========================================"

# Remove old module if still loaded
if lsmod | grep -q '^asgn1_driver '; then
    rmmod asgn1_driver
fi

echo
echo "===== 1. LOAD MODULE ====="

if insmod "$MODULE"; then
    ok "asgn1_driver loaded"
else
    bad "cannot load asgn1_driver"
    exit 1
fi

# Normally device_create + devtmpfs creates this automatically.
if [ ! -e "$DEV" ]; then
    mknod "$DEV" c 241 0
fi

if [ -e "$DEV" ]; then
    ok "/dev/asgn1 exists"
    ls -l "$DEV"
else
    bad "/dev/asgn1 does not exist"
fi

echo
echo "===== 2. GET VERSION ====="

VERSION=$($TEST version)
echo "$VERSION"

if echo "$VERSION" | grep -q "asgn1_driver v1.0"; then
    ok "GET_VERSION"
else
    bad "GET_VERSION"
fi

echo
echo "===== 3. GET STATS ====="

STATS=$($TEST stats)
echo "$STATS"

for field in open_count write_count read_count buffer_len last_write_size
do
    if echo "$STATS" | grep -q "$field"; then
        ok "GET_STATS contains $field"
    else
        bad "GET_STATS missing $field"
    fi
done

echo
echo "===== 4. MODE 0 - KERNEL MODE ====="

$TEST mode 0
echo -n "hello" > "$DEV"

MODE0=$(cat "$DEV")

echo "Read result: $MODE0"

if [ "$MODE0" = "[KERNEL]hello" ]; then
    ok "Mode 0 adds [KERNEL] prefix"
else
    bad "Mode 0 result = $MODE0"
fi

echo
echo "===== 5. MODE 1 - ECHO MODE ====="

$TEST mode 1
echo -n "hello" > "$DEV"

MODE1=$(cat "$DEV")

echo "Read result: $MODE1"

if [ "$MODE1" = "hello" ]; then
    ok "Mode 1 returns original data"
else
    bad "Mode 1 result = $MODE1"
fi

echo
echo "===== 6. RESET BUFFER ====="

echo -n "reset_me" > "$DEV"

BEFORE_RESET=$(cat "$DEV")
echo "Before reset: $BEFORE_RESET"

$TEST reset

AFTER_RESET=$(cat "$DEV")
echo "After reset: '$AFTER_RESET'"

if [ -z "$AFTER_RESET" ]; then
    ok "RESET_BUFFER cleared buffer"
else
    bad "buffer is not empty after reset"
fi

echo
echo "===== 7. STATS AFTER RESET ====="

RESET_STATS=$($TEST stats)
echo "$RESET_STATS"

if echo "$RESET_STATS" | grep -q 'write_count.*= 0'; then
    ok "write_count reset to 0"
else
    bad "write_count was not reset"
fi

if echo "$RESET_STATS" | grep -q 'read_count.*= 0'; then
    ok "read_count reset to 0"
else
    bad "read_count was not reset"
fi

if echo "$RESET_STATS" | grep -q 'buffer_len.*= 0'; then
    ok "buffer_len reset to 0"
else
    bad "buffer_len was not reset"
fi

if echo "$RESET_STATS" | grep -q 'last_write_size.*= 0'; then
    ok "last_write_size reset to 0"
else
    bad "last_write_size was not reset"
fi

echo
echo "===== 8. UNLOAD MODULE ====="

if rmmod asgn1_driver; then
    ok "asgn1_driver unloaded"
else
    bad "cannot unload asgn1_driver"
fi

if lsmod | grep -q '^asgn1_driver '; then
    bad "module still present in lsmod"
else
    ok "module removed from lsmod"
fi

if [ -e "$DEV" ]; then
    bad "/dev/asgn1 still exists"
else
    ok "/dev/asgn1 removed"
fi

echo
echo "===== 9. DMESG ====="
dmesg | grep asgn1 | tail -40

echo
echo "========================================"
echo "PASS = $PASS"
echo "FAIL = $FAIL"
echo "========================================"

if [ "$FAIL" -eq 0 ]; then
    echo "TASK 1 RESULT: PASS"
    exit 0
else
    echo "TASK 1 RESULT: FAIL"
    exit 1
fi
