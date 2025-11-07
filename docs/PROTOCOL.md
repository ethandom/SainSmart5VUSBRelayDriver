# PROTOCOL.md — SainSmart 4-Channel 5 V USB Relay (User-Space ASCII + Binary map)

This document defines a simple, stable, line-oriented ASCII protocol for controlling a SainSmart 4-channel 5 V USB relay board via a single device node (e.g., `/dev/usbrelay0`, `/dev/tty.usbserial*`). It also specifies the binary frame mapping commonly seen for these boards so your driver can translate cleanly.

The goals: human-readable commands, predictable responses, idempotent operations, and a minimal shim to the device’s byte protocol.

---

## 0) Transport

* Device node: one full-duplex byte stream (character device or serial TTY).
* Encoding: ASCII for commands, UTF-8 safe.
* Terminator: each command ends with `\n`.
* Whitespace: one or more spaces between tokens.
* Case: Commands and keywords are case-insensitive.
* Channel numbering: `1..4`.

---

## 1) Command Grammar (ASCII)

```
COMMAND   := SET | GET | GETALL | TOGGLE | MASK | RESET | PING | VERSION | HELP
SET       := "SET" SP CH SP STATE
GET       := "GET" SP CH
GETALL    := "GET" SP "ALL"
TOGGLE    := "TOGGLE" SP CH
MASK      := ("WRITE" | "READ") SP "MASK" [SP HEXMASK]
RESET     := "RESET"
PING      := "PING"
VERSION   := "VERSION"
HELP      := "HELP"

CH        := "1" | "2" | "3" | "4"
STATE     := "ON" | "OFF"
HEXMASK   := "0x" HEXDIGIT{1,2}
```

Example commands:

```
SET 1 ON
GET 2
GET ALL
TOGGLE 3
WRITE MASK 0x05
READ MASK
```

---

## 2) Responses (ASCII)

* Success: `OK`
* With values: `OK CH=<n> STATE=<ON|OFF>` or `OK MASK=0xHH`
* Errors: `ERR <CODE> <MESSAGE>`

Example:

```
> SET 1 ON
< OK CH=1 STATE=ON
```

---

## 3) Semantics

* SET is idempotent.
* GET returns current state.
* GET ALL returns 4-bit mask.
* TOGGLE flips one channel.
* WRITE MASK sets all relays at once.

---

## 4) Binary Frame Mapping

```
[ 0xFF, CHANNEL, VALUE ]
```

* `CHANNEL`: 0x01–0x04
* `VALUE`: 0x01 = ON, 0x00 = OFF

Mapping examples:

* `SET 1 ON` → `FF 01 01`
* `SET 3 OFF` → `FF 03 00`
* `WRITE MASK 0x05` → send per-bit frames for channels 1–4.

---

## 5) Notes

* Drivers must maintain a shadow state if hardware lacks readback.
* Apply safe mask (0x00) on device open to avoid boot chatter.
* Atomicity: WRITE MASK should occur in a single I/O batch.

---

Protocol version: `1.0`
License: MIT
