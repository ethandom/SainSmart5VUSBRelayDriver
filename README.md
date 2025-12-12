# SainSmart 4-Channel 5V USB Relay Driver

Custom Linux kernel driver and user-space tooling for a SainSmart 4-channel 5 V USB relay board (FT232R based).

The stack consists of:

* A kernel module (relay_driver.ko) that

  * Binds directly to the FTDI device (VID 0x0403, PID 0x6001)
  * Exposes a simple 1-byte mask ABI via /dev/usbrelayN

* A user-space CLI (relayctl) that

  * Speaks a line-oriented ASCII protocol
  * Translates commands like “set 1 on” into bit masks

For protocol details, see docs/PROTOCOL.md.

---

## 1. Repository layout

Typical structure (you may adjust as needed):

* kmod/

  * relay_driver.c – kernel module source
  * Makefile – builds relay_driver.ko

* userspace/

  * Makefile – builds user-space tools
  * include/usbrelay.h – shared constants/macros for user space
  * tools/relayctl.c – CLI front-end
  * tools/test_relayctl.sh – functional test script

* docs/

  * PROTOCOL.md – ASCII protocol + kernel mask ABI

* .gitignore, LICENSE, README.md – repository metadata

---

## 2. Requirements

* Linux kernel headers installed (matching your running kernel)
* A SainSmart-style FTDI USB relay with VID:PID 0403:6001
* Build tools: make, gcc
* Root access (for loading the kernel module and manipulating /dev)

---

## 3. Building

### 3.1 Kernel module

From kmod/:

cd kmod
make

If successful you should see a file:

relay_driver.ko

### 3.2 User-space tool

From userspace/tools/:

cd userspace/tools
make relayctl     (or just: make)

You should get:

./relayctl

---

## 4. Handling conflicting FTDI drivers

By default, generic FTDI drivers (ftdi_sio, usbserial) often bind to the device first.
For this project you want this driver (relay_driver) to bind instead.

### 4.1 Check currently loaded modules

lsmod | egrep 'ftdi_sio|usbserial'

If you see them loaded, you can temporarily remove them:

sudo rmmod ftdi_sio
sudo rmmod usbserial

This may affect other FTDI/serial devices attached to your system.
For a permanent setup you could add blacklist rules in /etc/modprobe.d/, but that is optional.

---

## 5. Loading the kernel module

From kmod/:

cd kmod
sudo insmod relay_driver.ko

Check kernel logs:

dmesg | grep usbrelay
or:
sudo journalctl -k | grep usbrelay

You should see messages about probe() and something like:

usbrelay: device initialized, /dev/usbrelay0 ready

Confirm the device node:

ls -l /dev/usbrelay*

Example:

crw-rw---- 1 root usbrelay 235, 0 Dec 11 20:45 /dev/usbrelay0

---

## 6. Permissions and groups

The device node is created with user root and group usbrelay (mode 0660).

To allow a non-root user to use relayctl:

1. Ensure the group exists:

   getent group usbrelay || sudo groupadd usbrelay

2. Add your user to that group:

   sudo usermod -aG usbrelay "$USER"

3. Log out and log back in so the new group membership is applied.

Verify:

id

You should see usbrelay in the groups list.
After that, you can use relayctl without sudo.

---

## 7. User-space CLI: relayctl

relayctl is the ASCII protocol client for /dev/usbrelay0.
It operates on a 4-bit mask:

* Bit 0 (0x01) → relay 1 (CH1)
* Bit 1 (0x02) → relay 2 (CH2)
* Bit 2 (0x04) → relay 3 (CH3)
* Bit 3 (0x08) → relay 4 (CH4)

### 7.1 Basic usage

General form:

./relayctl [options] <command> [args...]

Options:

* -d <device>   Override device path (default: /dev/usbrelay0)
* -i            Interactive REPL mode
* -v            Verbose prompt in interactive mode (adds “> ” before each line)

Commands:

* set <ch> <on|off>
  Set channel 1–4 ON or OFF.

* get <ch>
  Read state of a single channel.

* getall
  Print the current 4-bit mask.

* toggle <ch>
  Flip the state of channel <ch>.

* write-mask 0xHH
  Write a full 4-bit mask (0x00–0x0F).

* read-mask
  Read the current mask from the device.

* reset
  Turn all channels OFF (mask 0x00).

* ping
  Check that the device is reachable.

* version
  Print protocol/tool version.

* help
  Show usage and help text.

Examples:

Turn relay 1 ON:
./relayctl set 1 on

Get state of relay 1:
./relayctl get 1

Toggle relay 3:
./relayctl toggle 3

Set relays 2 and 4 ON, others OFF (0b1010 = 0x0A):
./relayctl write-mask 0x0A
./relayctl getall

### 7.2 Interactive REPL mode

Start REPL:

./relayctl -i

Then type commands (one per line):

getall
set 1 on
get 1
reset
getall
exit

The REPL uses the same parser and handlers as the one-shot CLI.
With -v you will see a “> ” prompt before each input line.

---

## 8. ASCII protocol summary

The full definition is in docs/PROTOCOL.md.
Quick summary:

* Commands are case-insensitive, one per line, for example:

  set 1 on
  get 2
  write-mask 0x05
  read-mask
  reset
  ping

* Typical success responses:

  OK
  OK CH=<n> STATE=<ON|OFF>
  OK MASK=0xHH
  OK VERSION=<ver> TOOL=<tool>

* Typical error responses:

  ERR BAD_COMMAND ...
  ERR BAD_CHANNEL ...
  ERR BAD_STATE ...
  ERR BAD_MASK ...
  ERR DEVICE_UNAVAILABLE ...
  ERR INTERNAL_ERROR ...

Internally, the kernel ABI is just a 1-byte read/write mask; no extra framing.

---

## 9. Testing with test_relayctl.sh

The script userspace/tools/test_relayctl.sh exercises most functionality:

* ping, version, help
* reset, set, get, getall, toggle, write-mask, read-mask
* Error paths: bad channels, bad masks, unknown commands
* Interactive mode: drives the REPL via stdin
* -d device override: tests default path, a symlink, and a missing device

Run:

cd userspace/tools
./test_relayctl.sh

Look through the output to verify:

* Masks and channel states match expectations
* Error cases return the correct “ERR <CODE>” format
* Exit statuses are 0 on success and non-zero on failure

---

## 10. Unloading the driver

To detach the driver and remove device nodes:

sudo rmmod relay_driver

Check that /dev/usbrelay* has disappeared and that logs show the disconnect:

dmesg | egrep 'usbrelay|relay_driver'

If you want the generic FTDI drivers back:

sudo modprobe usbserial
sudo modprobe ftdi_sio

---

## 11. License

* Kernel module: MODULE_LICENSE("GPL").
* Userspace tools and protocol: see source headers or LICENSE.
