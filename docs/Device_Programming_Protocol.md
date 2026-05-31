# TP-CAN-2I — Device Programming Protocol (TP-DPP) v1.0

**Tester Present Specialist Automotive Solutions** · © 2026 Jack Leighton · Designed in Australia

This is the **single source of truth** for how the PC software and the device firmware talk to
each other for **reading, writing, and flashing firmware/config**. The firmware (ESP32-S3) and the
.NET 4.8.1 configurator MUST both implement this document exactly. It is transport-agnostic: the
same Application PDU rides over **CAN (via J2534 ISO-TP)**, **Serial UART**, and **USB-C CDC**.

---

## 1. Design goals

- **One command set, three transports.** Identical request/response PDUs on CAN, UART, USB-C.
- **J2534-native.** The CAN transport is ISO 15765-2 (ISO-TP), which every J2534 PassThru driver
  segments/reassembles for free — so the PC side is just `J2534Manager.SendReceive(0x7FE, pdu)`.
- **UDS-flavoured.** Service IDs mirror a subset of ISO 14229 (UDS) so the flow is familiar and
  tooling-friendly, but the ID space (0x7FE/0x7FA), DIDs, and routines are our own.
- **Safe field updates.** Firmware is written to the **inactive OTA slot**, CRC-verified, then
  activated on the next reset. A bad image never bricks the unit (rolls back to the known-good slot).

---

## 2. Transport layer

### 2.1 CAN (J2534 / ISO 15765-2)
| Item | Value |
|---|---|
| Bus | 500 kbit/s classic CAN 2.0B, 11-bit IDs |
| **Request ID (PC → device)** | **0x7FE** (PC *transmits* on 0x7FE) |
| **Response ID (device → PC)** | **0x7FA** (PC *receives* on 0x7FA) |
| Addressing | ISO-TP normal addressing, FRAME_PAD (pad to 8 bytes, 0xAA) |
| Max PDU | 4095 bytes (ISO-TP limit); we cap blocks well under this (§6) |
| Which bus | Firmware listens for 0x7FE on **CAN1 (vehicle/upstream)** by default; also
selectable on CAN2 via config DID `0x0103`. |

> From the PC's point of view this is exactly **"RX 0x7FA / TX 0x7FE"**. On the J2534 side, open
> ISO15765 @ 500k and install a flow-control filter with `txId=0x7FE, rxId=0x7FA`, then use the
> existing `J2534Manager.SendReceive(0x7FE, pdu)` — ISO-TP multiframe is handled by the adapter.

### 2.2 Serial UART  &  2.3 USB-C CDC
Both use the **same framing** (USB-C is the ESP32-S3 native USB CDC virtual COM port; UART is the
harness service lines pins 10/11/12 or service header J3).

| Item | Value |
|---|---|
| UART baud | 921600, 8N1, no flow control (USB-CDC baud is irrelevant/ignored) |
| Frame | `MAGIC(2) | LEN(2) | PDU(LEN) | CRC16(2)` |
| MAGIC | `0x54 0x50` (`"TP"`) |
| LEN | big-endian, length of PDU only (0…4095) |
| CRC16 | CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF), big-endian, computed over **MAGIC+LEN+PDU** |
| Resync | On CRC mismatch the receiver slides forward one byte and re-scans for MAGIC. |

No byte-stuffing is used; the length + CRC + magic resync keep the stream self-correcting.

---

## 3. Application PDU (transport-agnostic)

```
Request : [ SID ] [ param bytes … ]
Positive: [ SID + 0x40 ] [ data bytes … ]
Negative: [ 0x7F ] [ SID ] [ NRC ]
```

Multi-byte fields are **big-endian**. The PDU is the payload of the CAN ISO-TP message *or* the
`PDU` field of a serial frame — never both wrapped together.

---

## 4. Service catalog

| SID | Name | Request params | Positive response |
|---|---|---|---|
| 0x10 | SESSION_CONTROL | `subfn` (01=default, 02=programming) | `subfn` + `P2(2)` + `P2*(2)` ms |
| 0x11 | ECU_RESET | `subfn` (01=hardReset, 03=jumpToApp) | `subfn` |
| 0x27 | SECURITY_ACCESS | `01`=requestSeed → / `02`=sendKey+key(4) | seed(4) / (empty) |
| 0x3E | TESTER_PRESENT | `00` | `00` |
| 0x22 | READ_DATA_BY_ID | `DID(2)` | `DID(2)` + data |
| 0x2E | WRITE_DATA_BY_ID | `DID(2)` + data | `DID(2)` |
| 0x23 | READ_MEMORY | `addr(4)` + `len(2)` | data (`len` bytes, len ≤ 1024) |
| 0x31 | ROUTINE_CONTROL | `01`=start `routineId(2)` [+opt] | `01` + `routineId(2)` + `status(1)` [+result] |
| 0x34 | REQUEST_DOWNLOAD | `fmt(1)` `alfid(1)` `addr(4)` `size(4)` `crc32(4)` | `lenFmt(1)` + `maxBlock(2)` |
| 0x36 | TRANSFER_DATA | `bsc(1)` + data | `bsc(1)` |
| 0x37 | REQUEST_TRANSFER_EXIT | (none) | `status(1)` |

`SECURITY_ACCESS` is **optional** (compile flag `DPP_SECURITY`). Default bench build is open
(seed returns 0, any key accepted) so it never blocks development; production may enable a real
seed/key. `fmt`/`alfid` follow UDS convention; for us `fmt=0x00`, `alfid=0x44` (4-byte addr + 4-byte size).

### 4.1 Data Identifiers (DIDs, for 0x22 / 0x2E)
| DID | R/W | Meaning |
|---|---|---|
| 0xF180 | R | Bootloader/protocol version (ASCII, e.g. `TP-DPP 1.0`) |
| 0xF181 | R | Application firmware version (ASCII semver + build date) |
| 0xF18A | R | Device serial number (12 bytes) |
| 0xF190 | R | Chip ID / Wi-Fi MAC (6 bytes) |
| 0xF1A0 | R | Partition map: `running(1) slotA_size(4) slotB_size(4) appUsed(4)` |
| 0xF1B0 | R | Active/running OTA slot (0 or 1) |
| 0x0100 | R/W | Device config blob (CAN bitrates, MITM mode, name) — see §8 |
| 0x0103 | R/W | Programming-bus select: 0=CAN1, 1=CAN2 |

### 4.2 Routines (for 0x31, sub 0x01 = startRoutine)
| routineId | Meaning | Extra req | status |
|---|---|---|---|
| 0xFF01 | Erase inactive OTA slot | — | 0x00 ok / 0x01 busy / 0x02 fail |
| 0xFF02 | Verify staged image CRC32 | `crc32(4)` (expected) | 0x00 match / 0x03 mismatch |
| 0xFF03 | Activate staged image (set boot + mark valid) | — | 0x00 ok |
| 0x0202 | Self-test (CAN1/CAN2 loopback, isolation rails) | — | bitmask result(1) |

---

## 5. Negative Response Codes (NRC)
| NRC | Meaning |
|---|---|
| 0x10 | generalReject |
| 0x11 | serviceNotSupported |
| 0x13 | incorrectMessageLengthOrInvalidFormat |
| 0x22 | conditionsNotCorrect (e.g. not in programming session) |
| 0x24 | requestSequenceError (e.g. TRANSFER_DATA before REQUEST_DOWNLOAD) |
| 0x31 | requestOutOfRange (addr/len/DID) |
| 0x35 | invalidKey (security) |
| 0x72 | programmingFailure (flash write/erase error) |
| 0x73 | wrongBlockSequenceCounter |
| 0x78 | responsePending (firmware busy; PC waits up to P2*) |

---

## 6. Canonical flashing sequence (write/update firmware)

The PC turns a firmware image (`.bin`, or `.hex`→bin) into this exchange:

1. `10 02` — enter **programming session** (resp gives P2/P2* timing).
2. `27 01` → seed; `27 02 <key>` — security (no-op on open builds).
3. `31 01 FF01` — **erase** inactive OTA slot (firmware replies `78` responsePending then `00`).
4. `34 00 44 <addr=00000000> <size> <crc32>` — **request download**; device replies `maxBlock`
   (e.g. 0x0200 = 512). `addr=0` means "the inactive OTA slot".
5. Loop **`36 <bsc> <chunk>`** with `chunk ≤ maxBlock`, `bsc` = 1,2,3…wrapping 0xFF→0x00. Device
   echoes `bsc`. (Firmware streams each chunk straight into `esp_ota_write`.)
6. `37` — **request transfer exit** (device finalises the OTA write).
7. `31 01 FF02 <crc32>` — **verify** staged image CRC32 (must match what was sent in step 4).
8. `31 01 FF03` — **activate** (set boot partition, mark OTA valid).
9. `11 01` — **hard reset**; unit reboots into the new image. If the new image fails to confirm
   itself healthy, the ESP32 rolls back to the previous slot automatically.

Progress % = bytes sent / total size. PC keeps the session alive with `3E 00` if a step approaches P2*.

## 6b. Reading firmware / config

- **Read identity:** `22 F181` (app version), `22 F18A` (serial), `22 F1A0` (partition map).
- **Read memory/flash region:** `23 <addr(4)> <len(2)>` in ≤1024-byte windows; PC concatenates.
  (Used to dump the running app slot for backup before an update.)
- **Read config:** `22 0100`; **write config:** `2E 0100 <blob>` (no reflash needed).

---

## 7. Timing
| Param | Value | Use |
|---|---|---|
| P2 | 50 ms | normal request→response |
| P2* | 5000 ms | extended (erase/write/verify), signalled by NRC 0x78 |
| S3 (tester present) | send `3E 00` every 2000 ms while a programming session is open |
| Inter-block | none required; device flow-controls via ISO-TP / serial CRC-NAK |

---

## 8. Device config blob (DID 0x0100) layout
```
offset  size  field
0       1     version (=1)
1       1     mitm_mode      0=passthru 1=filter 2=rewrite 3=block
2       4     can1_bitrate   (bit/s, e.g. 500000)
6       4     can2_bitrate
10      1     can_fd_enable  bit0=CAN1 FD, bit1=CAN2 FD
11      4     can_fd_data_bitrate
15      1     term1_enable   on-board 120R CAN1 (0/1)
16      1     term2_enable   on-board 120R CAN2 (0/1)
17      16    device_name    ASCII, null-padded
33      2     crc16          CRC-16/CCITT over bytes 0..32
```

---

## 9. Worked examples

**CAN (bytes on the wire are ISO-TP-segmented by the adapter; PDUs shown logically):**
```
PC → 0x7FE : 10 02                      enter programming session
dev→ 0x7FA : 50 02 00 32 13 88          P2=50ms P2*=5000ms
PC → 0x7FE : 34 00 44 00 00 00 00 00 01 00 00 1A 2B 3C 4D   download 64 KiB, crc 0x1A2B3C4D
dev→ 0x7FA : 74 02 02 00                 maxBlock = 0x0200 (512)
PC → 0x7FE : 36 01 <512 bytes>          first block
dev→ 0x7FA : 76 01
...                                      (repeat, bsc++)
PC → 0x7FE : 37                          transfer exit
dev→ 0x7FA : 77 00
PC → 0x7FE : 31 01 FF02 1A 2B 3C 4D      verify crc
dev→ 0x7FA : 71 01 FF02 00               match
PC → 0x7FE : 31 01 FF03                  activate
dev→ 0x7FA : 71 01 FF03 00
PC → 0x7FE : 11 01                       reset → boots new image
dev→ 0x7FA : 51 01
```

**Serial/USB-CDC** wraps each identical PDU as `54 50 | LEN | PDU | CRC16`. Example `3E 00`:
```
54 50 00 02 3E 00 <crc16>
```

---

## 10. Versioning
The protocol version is read via `22 F180`. Minor additions (new DIDs/routines) keep `1.x`;
breaking changes bump the major. Firmware and PC software both advertise their supported version
in their About/identity strings.
```
