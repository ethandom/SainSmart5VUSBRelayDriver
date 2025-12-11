#!/usr/bin/env bash
# test_relayctl.sh
#
# Simple but fairly thorough test harness for relayctl + usbrelay kmod.
# Run from the directory where ./relayctl lives.
#
# Usage:
#   ./test_relayctl.sh
#
# Assumptions:
#   - /dev/usbrelay0 exists and is bound to your kernel driver
#   - Your user has permission to open /dev/usbrelay0 (group "usbrelay" etc.)

set -u  # treat unset vars as error, but DO NOT use -e (we want all tests to run)

RELAYCTL="./relayctl"
DEVICE="/dev/usbrelay0"

echo "=== relayctl test harness ==="
echo "Using binary: ${RELAYCTL}"
echo "Using device: ${DEVICE}"
echo

if [ ! -x "${RELAYCTL}" ]; then
    echo "ERROR: ${RELAYCTL} not found or not executable"
    exit 1
fi

if [ ! -e "${DEVICE}" ]; then
    echo "ERROR: ${DEVICE} does not exist"
    ls -l /dev/usbrelay*
    exit 1
fi

echo "Device node:"
ls -l "${DEVICE}"
echo

run_test() {
    local name="$1"; shift
    echo "=================================================="
    echo "TEST: ${name}"
    echo "CMD : $*"
    echo "--------------------------------------------------"
    "$@"
    local status=$?
    echo "--------------------------------------------------"
    echo "Exit status: ${status}"
    echo
}

# 1) Basic ping / version / help
run_test "ping"           "${RELAYCTL}" ping
run_test "version"        "${RELAYCTL}" version
run_test "help (short)"   "${RELAYCTL}" help

# 2) Reset and read mask
run_test "reset (all off)"          "${RELAYCTL}" reset
run_test "getall after reset"       "${RELAYCTL}" getall

# 3) Set individual channels ON, then read back via get/getall
run_test "set CH1 ON"               "${RELAYCTL}" set 1 on
run_test "get CH1"                  "${RELAYCTL}" get 1
run_test "getall after CH1 ON"      "${RELAYCTL}" getall

run_test "set CH3 ON"               "${RELAYCTL}" set 3 on
run_test "get CH3"                  "${RELAYCTL}" get 3
run_test "getall after CH1+CH3 ON"  "${RELAYCTL}" getall

# 4) Toggle a channel
run_test "toggle CH1"               "${RELAYCTL}" toggle 1
run_test "get CH1 after toggle"     "${RELAYCTL}" get 1
run_test "getall after toggle CH1"  "${RELAYCTL}" getall

# 5) Write / read mask explicitly
run_test "write-mask 0x0A"          "${RELAYCTL}" write-mask 0x0A
run_test "read-mask (expect 0x0A)"  "${RELAYCTL}" read-mask
run_test "get CH1 (0x0A => OFF)"    "${RELAYCTL}" get 1
run_test "get CH2 (0x0A => ON)"     "${RELAYCTL}" get 2
run_test "get CH3 (0x0A => OFF)"    "${RELAYCTL}" get 3
run_test "get CH4 (0x0A => ON)"     "${RELAYCTL}" get 4

# 6) Error handling: bad channels, bad mask, bad commands
run_test "bad channel (set 0 on)"      "${RELAYCTL}" set 0 on
run_test "bad channel (set 5 on)"      "${RELAYCTL}" set 5 on
run_test "bad mask (write-mask 0x10)"  "${RELAYCTL}" write-mask 0x10
run_test "bad mask (write-mask xyz)"   "${RELAYCTL}" write-mask xyz
run_test "bad command name"            "${RELAYCTL}" frobnicate

# 7) Make sure we can still talk to the device after errors
run_test "reset after error tests"     "${RELAYCTL}" reset
run_test "getall after final reset"    "${RELAYCTL}" getall

# 8) Interactive / REPL sanity test:
#    Send a few commands through stdin and see that they all work.
echo "=================================================="
echo "TEST: interactive REPL basic"
echo "CMD : printf 'getall\nset 1 on\ngetall\nreset\ngetall\nexit\n' | ${RELAYCTL} -i"
echo "--------------------------------------------------"
printf 'getall\nset 1 on\ngetall\nreset\ngetall\nexit\n' | "${RELAYCTL}" -i
status=$?
echo "--------------------------------------------------"
echo "Exit status: ${status}"
echo

# 9) -d flag tests (device override)
echo "=================================================="
echo "TEST GROUP: -d (device override)"
echo

# 9.1 -d with the default device path (should behave exactly like no -d)
run_test "-d with default device" \
    "${RELAYCTL}" -d "${DEVICE}" getall

# 9.2 -d with a symlink to the real device (tests path override logic)
ALT_DEVICE="/tmp/usbrelay0-alt"
echo "Creating symlink ${ALT_DEVICE} -> ${DEVICE}"
ln -sf "${DEVICE}" "${ALT_DEVICE}"
ls -l "${ALT_DEVICE}"
echo

run_test "-d with symlink (set 1 on)" \
    "${RELAYCTL}" -d "${ALT_DEVICE}" set 1 on

run_test "-d with symlink (getall)" \
    "${RELAYCTL}" -d "${ALT_DEVICE}" getall

# 9.3 -d with a bogus device (should fail with DEVICE_UNAVAILABLE)
run_test "-d with missing device" \
    "${RELAYCTL}" -d "/dev/usbrelay-does-not-exist" ping

echo "=== End of relayctl tests ==="
