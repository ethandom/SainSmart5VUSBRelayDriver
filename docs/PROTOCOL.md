# PROTOCOL.md - SainSmart 4-Channel 5V USB Relay

(Line-Oriented ASCII Protocol + Kernel Mask ABI)

This document defines:

1. A line-oriented ASCII protocol for controlling a 4-relay SainSmart USB board.
2. The underlying kernel ABI: a 1-byte mask that drives the FTDI GPIO pins.

=========================================
0. TRANSPORT AND MODEL
=========================================

* Channel: full-duplex byte stream (for example /dev/usbrelay0, /dev/ttyUSB*).
* Encoding: ASCII (UTF-8 safe).
* Commands: one per line, terminated by "\n".
* Whitespace: one or more spaces between tokens.
* Case: commands and keywords are case-insensitive.
* Relays: 4 channels, numbered 1..4.

Mask layout (shadow state M, 4 bits used):

* Bit 0 (0x01) -> CH1
* Bit 1 (0x02) -> CH2
* Bit 2 (0x04) -> CH3
* Bit 3 (0x08) -> CH4

All higher bits must be 0.

=========================================
1. ASCII COMMAND PROTOCOL
=========================================

1.1 Grammar (informal)

COMMAND := SET | GET | GETALL | TOGGLE | WRITE-MASK | READ-MASK | RESET | PING | VERSION | HELP

SET        := "SET" SP CH SP STATE
GET        := "GET" SP CH
GETALL     := "GETALL"
TOGGLE     := "TOGGLE" SP CH
WRITE-MASK := "WRITE-MASK" SP HEXMASK
READ-MASK  := "READ-MASK"
RESET      := "RESET"
PING       := "PING"
VERSION    := "VERSION"
HELP       := "HELP"

CH      := "1" | "2" | "3" | "4"
STATE   := "ON" | "OFF"
HEXMASK := "0x" HEXDIGIT{1,2}
SP      := one or more spaces

Example commands:

SET 1 ON
GET 2
GETALL
TOGGLE 3
WRITE-MASK 0x05
READ-MASK
RESET
PING
VERSION
HELP

---

## 1.2 Semantics (in terms of mask M)

Let:

* bit(CH) = (1U << (CH - 1))
* M be the current mask (0x00-0x0F)

SET CH ON
M := M | bit(CH)
Apply M to hardware.

SET CH OFF
M := M & ~bit(CH)
Apply M to hardware.

GET CH
Optionally refresh M from kernel (see section 2).
STATE := ON if (M & bit(CH)) != 0, else OFF.

GETALL
Optionally refresh M.
Response is based on current M.

TOGGLE CH
M := M ^ bit(CH)
Apply M to hardware.

WRITE-MASK 0xHH
Parse HH as hex.
If (HH & ~0x0F) != 0:
return ERR BAD_MASK (reject out-of-range mask).
Else:
M := HH
Apply M to hardware.

READ-MASK
Refresh M if possible.
Respond with current M.

RESET
M := 0x00 (all relays OFF)
Apply M to hardware.

PING
No state change; connectivity / health check only.

VERSION
HELP
Informational only; no state change.

SET, WRITE-MASK, and RESET are idempotent.

---

## 1.3 Responses

All responses are a single ASCII line terminated by "\n".

1.3.1 Success responses

Typical patterns:

OK
OK CH=<n> STATE=<ON|OFF>
OK MASK=0xHH
OK VERSION=<ver> TOOL=<tool>
OK <other-info>

Examples:

> SET 1 ON
< OK CH=1 STATE=ON

> GETALL
< OK MASK=0x05

1.3.2 Error responses

Pattern:

ERR <CODE> <MESSAGE>

Codes:

BAD_COMMAND        - unknown command or syntax error
BAD_CHANNEL        - channel not 1..4
BAD_STATE          - state not ON/OFF
BAD_MASK           - invalid or out-of-range mask
DEVICE_UNAVAILABLE - underlying I/O error
INTERNAL_ERROR     - unexpected failure

Examples:

> SET 5 ON
< ERR BAD_CHANNEL Channel must be 1..4

> WRITE-MASK xyz
< ERR BAD_MASK Mask must be 0xHH

=========================================
2. KERNEL / DRIVER MASK ABI
=========================================

The ASCII layer is implemented on top of a simple kernel ABI:

* Device: /dev/usbrelayN (character device).
* ABI: read/write a single byte mask.

---

## 2.1 Write (set mask)

User-space (ASCII handler):

uint8_t mask = M & 0x0F;
write(fd, &mask, 1);

Driver semantics:

* write(fd, buf, 1):
  M := buf[0] & 0x0F
  Push M to FTDI (bit-bang / SetBitMode).
* count == 0  -> return -EINVAL
* hardware failure -> return negative errno (for example -EIO)

---

## 2.2 Read (get mask)

User-space:

uint8_t mask;
read(fd, &mask, 1);

Driver semantics:

* read(fd, buf, 1):
  Return current shadow mask M (one byte).
* If hardware does not support read-back, M is whatever was last written successfully.

---

## 2.3 Hardware mapping (FTDI)

* FTDI output byte = M (0x00-0x0F).
* Bit 0 -> Relay 1
* Bit 1 -> Relay 2
* Bit 2 -> Relay 3
* Bit 3 -> Relay 4

There is no extra framing. The relay state is fully determined by this byte.

---

## 2.4 Initialization policy

* On probe:
  M := 0x00
  Apply M to hardware (all OFF).

* On first open:
  Optionally re-apply M := 0x00.

* On close:
  Either leave relays as-is,
  or reset to 0x00 (implementation choice; document clearly).

=========================================
3. META
=========================================

Protocol version: 1.1
License: MIT