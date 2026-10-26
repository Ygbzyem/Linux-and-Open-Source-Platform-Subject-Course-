#!/bin/sh
# ============================================================================
# test_counter.sh - Automated test for counter_driver (ASSIGNMENT-01, Task 2)
# Author : Pham Minh Tung - SE204386
# Run    : inside QEMU (BusyBox ash) after the rootfs contains counter_driver.ko
# Result : [PASS]/[FAIL] per check, exit code = number of failed checks
# ============================================================================

KO=/lib/modules/5.15.221/counter_driver.ko
DEV=/dev/counter
PROC=/proc/counter_info

PASS=0
FAIL=0

pass() { echo "  [PASS] $1"; PASS=$((PASS + 1)); }
fail() { echo "  [FAIL] $1"; FAIL=$((FAIL + 1)); }

# expect_eq <description> <expected> <actual>
expect_eq() {
    if [ "$2" = "$3" ]; then pass "$1"; else fail "$1 (expected [$2], got [$3])"; fi
}

# expect_ok <description> <command...>   : command must succeed
expect_ok() {
    desc=$1; shift
    if "$@" >/dev/null 2>&1; then pass "$desc"; else fail "$desc"; fi
}

# expect_err <description> <command...>  : command must FAIL
expect_err() {
    desc=$1; shift
    if "$@" >/dev/null 2>&1; then fail "$desc (unexpectedly succeeded)"; else pass "$desc"; fi
}

show_proc() {
    echo "  --- $PROC ($1) ---"
    sed 's/^/      /' "$PROC"
}

echo "================================================================"
echo " ASSIGNMENT-01 Task 2 - counter_driver (misc device) test"
echo "================================================================"

# --- 0. Start from a clean state --------------------------------------------
grep -q '^counter_driver ' /proc/modules && rmmod counter_driver

# --- 1. Load ------------------------------------------------------------------
echo "[1] Load module"
expect_ok  "insmod counter_driver.ko"           insmod "$KO"
expect_ok  "$DEV is a character device"         [ -c "$DEV" ]
expect_ok  "registered in /proc/misc"           grep -q ' counter$' /proc/misc
expect_ok  "misc major is 10"                   sh -c "ls -l $DEV | grep -q ' 10, '"
expect_ok  "$PROC exists"                       [ -r "$PROC" ]
ls -l "$DEV" | sed 's/^/      /'

# --- 2. Read returns the value, then increments -----------------------------
echo "[2] Read auto-increment"
expect_eq  "1st read = 0"                       "0" "$(cat $DEV)"
expect_eq  "2nd read = 1"                       "1" "$(cat $DEV)"
expect_eq  "3rd read = 2"                       "2" "$(cat $DEV)"

# --- 3. Statistics before any write ------------------------------------------
echo "[3] /proc/counter_info before write"
show_proc "before write"
expect_ok  "total reads = 3"                    grep -q 'Total reads *: 3$'  "$PROC"
expect_ok  "total writes = 0"                   grep -q 'Total writes *: 0$' "$PROC"

# --- 4. Write sets the value ---------------------------------------------------
echo "[4] Write 100"
expect_ok  "echo 100 accepted"                  sh -c "echo 100 > $DEV"
expect_eq  "read after set = 100"               "100" "$(cat $DEV)"
expect_eq  "next read = 101"                    "101" "$(cat $DEV)"
show_proc "after write"

# --- 5. Reset to 0 -------------------------------------------------------------
echo "[5] Reset (write 0)"
expect_ok  "echo 0 accepted"                    sh -c "echo 0 > $DEV"
expect_eq  "read after reset = 0"               "0" "$(cat $DEV)"
show_proc "after reset"

# --- 6. Invalid input ----------------------------------------------------------
echo "[6] Invalid input (-EINVAL)"
expect_err "non-numeric 'abc' rejected"         sh -c "echo abc > $DEV"
expect_err "negative '-5' rejected"             sh -c "echo -5 > $DEV"
expect_eq  "value unchanged after bad writes"   "1" "$(cat $DEV)"

# --- 7. *ppos handling with a 1-byte buffer ----------------------------------
echo "[7] Small-buffer read (dd bs=1) - one value, one increment"
expect_eq  "dd bs=1 returns whole value"        "2" "$(dd if=$DEV bs=1 2>/dev/null)"
expect_eq  "only one increment for that read"   "3" "$(cat $DEV)"

# --- 8. Final statistics -------------------------------------------------------
echo "[8] Final statistics"
show_proc "final"
expect_ok  "total reads = 9"                    grep -q 'Total reads *: 9$'  "$PROC"
expect_ok  "total writes = 2 (invalid ones not counted)" grep -q 'Total writes *: 2$' "$PROC"

# --- 9. Unload -----------------------------------------------------------------
echo "[9] Unload module"
expect_ok  "rmmod counter_driver"               rmmod counter_driver
expect_err "module no longer listed"            grep -q '^counter_driver ' /proc/modules
expect_err "$DEV removed"                       [ -e "$DEV" ]

echo "[10] Kernel log (last 12 counter lines)"
dmesg | grep 'counter:' | tail -12 | sed 's/^/      /'

echo "================================================================"
echo " RESULT: $PASS passed, $FAIL failed"
echo "================================================================"
exit $FAIL
