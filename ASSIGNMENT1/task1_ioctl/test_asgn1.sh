#!/bin/sh
# ============================================================================
# test_asgn1.sh - Kiem thu tu dong asgn1_driver (ASSIGNMENT-01, Task 1)
# Author : Pham Minh Tung - SE204386
# Chay   : trong QEMU (BusyBox ash), sau khi rootfs co asgn1_driver.ko va
#          /usr/bin/asgn1_app
# Ket qua: in [PASS]/[FAIL] cho tung muc, exit code = so muc FAIL
# ============================================================================

KO=/lib/modules/5.15.221/asgn1_driver.ko
DEV=/dev/asgn1
PROC=/proc/asgn1_info
MAJOR=245

PASS=0
FAIL=0

# pass/fail <mo ta>
pass() { echo "  [PASS] $1"; PASS=$((PASS + 1)); }
fail() { echo "  [FAIL] $1"; FAIL=$((FAIL + 1)); }

# expect_eq <mo ta> <gia tri mong doi> <gia tri thuc te>
expect_eq() {
    if [ "$2" = "$3" ]; then
        pass "$1"
    else
        fail "$1 (expected [$2], got [$3])"
    fi
}

# expect_ok <mo ta> <lenh...>   : lenh phai thanh cong (exit 0)
expect_ok() {
    desc=$1; shift
    if "$@" >/dev/null 2>&1; then pass "$desc"; else fail "$desc"; fi
}

# expect_err <mo ta> <lenh...>  : lenh phai THAT BAI (exit != 0)
expect_err() {
    desc=$1; shift
    if "$@" >/dev/null 2>&1; then fail "$desc (unexpectedly succeeded)"; else pass "$desc"; fi
}

echo "================================================================"
echo " ASSIGNMENT-01 Task 1 - asgn1_driver ioctl test"
echo "================================================================"

# --- 0. Don dep: neu module dang nap thi go ra de test tu trang thai sach ---
grep -q '^asgn1_driver ' /proc/modules && rmmod asgn1_driver

# --- 1. Nap module -----------------------------------------------------------
echo "[1] Load module"
expect_ok  "insmod asgn1_driver.ko"             insmod "$KO"
expect_ok  "$DEV is a character device"         [ -c "$DEV" ]
expect_ok  "major $MAJOR registered"            grep -q "^ *$MAJOR asgn1\$" /proc/devices
expect_ok  "$PROC exists"                       [ -r "$PROC" ]

# --- 2. Mode mac dinh = 1 (ECHO) --------------------------------------------
echo "[2] Default mode (ECHO)"
echo hello > "$DEV"
expect_eq  "echo mode stores data unchanged"    "hello" "$(cat $DEV)"

# --- 3. GET_VERSION ----------------------------------------------------------
echo "[3] ASGN1_GET_VERSION"
VER=$(asgn1_app version 2>&1)
echo "      $VER"
case "$VER" in
    *SE204386*) pass "version string contains student ID" ;;
    *)          fail "version string contains student ID" ;;
esac

# --- 4. SET_MODE 0 (KERNEL) --------------------------------------------------
echo "[4] ASGN1_SET_MODE 0 (KERNEL prefix)"
expect_ok  "set mode 0"                         asgn1_app mode 0
echo hello > "$DEV"
expect_eq  "kernel mode adds [KERNEL] prefix"   "[KERNEL] hello" "$(cat $DEV)"

# --- 5. GET_STATS ------------------------------------------------------------
echo "[5] ASGN1_GET_STATS"
STATS=$(asgn1_app stats 2>&1)
echo "$STATS" | sed 's/^/      /'
expect_ok  "buffer_len = 15 (9 prefix + 6 user)" sh -c "echo \"$STATS\" | grep -q 'buffer_len *= 15\$'"
expect_ok  "last_write_size = 6 (user bytes only)" sh -c "echo \"$STATS\" | grep -q 'last_write_size *= 6\$'"

# --- 6. SET_MODE 1 (ECHO) ----------------------------------------------------
echo "[6] ASGN1_SET_MODE 1 (back to ECHO)"
expect_ok  "set mode 1"                         asgn1_app mode 1
echo hello > "$DEV"
expect_eq  "echo mode restored"                 "hello" "$(cat $DEV)"

# --- 7. Xu ly loi ------------------------------------------------------------
echo "[7] Error handling"
expect_err "SET_MODE 5 rejected (-EINVAL)"      asgn1_app mode 5
expect_err "unknown ioctl rejected (-ENOTTY)"   asgn1_app invalid
expect_ok  "dmesg logs unknown command"         sh -c "dmesg | grep -q 'asgn1_driver: ioctl unknown command'"

# --- 8. Ghi qua dai: cat bot, khong ghi lap ---------------------------------
echo "[8] Oversized write (2000 bytes)"
head -c 2000 /dev/zero | tr '\0' x > "$DEV"
expect_ok  "buffer capped at 1023 bytes"        grep -q 'Data length *: 1023 bytes' "$PROC"
expect_ok  "single write call (no retry overwrite)" sh -c "dmesg | grep -q 'received 2000 bytes'"

# --- 9. RESET_BUFFER ---------------------------------------------------------
echo "[9] ASGN1_RESET_BUFFER"
echo "data before reset" > "$DEV"
expect_ok  "reset"                              asgn1_app reset
expect_eq  "buffer empty after reset"           "" "$(cat $DEV)"
STATS=$(asgn1_app stats 2>&1)
expect_ok  "write_count = 0 after reset"        sh -c "echo \"$STATS\" | grep -q 'write_count *= 0\$'"
expect_ok  "read_count = 0 after reset"         sh -c "echo \"$STATS\" | grep -q 'read_count *= 0\$'"

# --- 10. Go module -----------------------------------------------------------
echo "[10] Unload module"
expect_ok  "rmmod asgn1_driver"                 rmmod asgn1_driver
expect_err "module no longer listed"            grep -q '^asgn1_driver ' /proc/modules
expect_err "$DEV removed"                       [ -e "$DEV" ]

# --- 11. dmesg ---------------------------------------------------------------
echo "[11] Kernel log (last 15 asgn1 lines)"
dmesg | grep 'asgn1_driver' | tail -15 | sed 's/^/      /'

# --- Tong ket ----------------------------------------------------------------
echo "================================================================"
echo " RESULT: $PASS passed, $FAIL failed"
echo "================================================================"
exit $FAIL
