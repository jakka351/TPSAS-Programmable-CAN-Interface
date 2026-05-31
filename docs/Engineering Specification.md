# Tester Present — Programmable CAN Interface
## Master Engineering Specification

| | |
|---|---|
| **Product** | Programmable Dual-CAN Inline Interface (CAN / Serial / USB / Wireless) |
| **Model** | TP-CAN-2I (Tester Present Specialist Automotive Solutions) |
| **Document** | Master Engineering Specification |
| **Revision** | A (2026-05-31) |
| **Author** | Jack Leighton — Tester Present Specialist Automotive Solutions |
| **Copyright** | © 2026 Jack Leighton. All rights reserved. |
| **Origin** | Designed in Australia |

---

## 1. Product Overview

The TP-CAN-2I is a ruggedised, automotive-grade **inline CAN-bus interface** that installs
between a vehicle harness (upstream) and an ECU/module (downstream). It can **monitor,
pass-through, filter, and optionally rewrite** CAN traffic flowing between the two network
segments — a configurable "man-in-the-middle" for diagnostics, emulation, data-logging,
gateway/translation, and bench development.

It is built around an **ESP32-S3** providing Wi-Fi (telemetry / OTA) and Bluetooth LE
(configuration), with **two fully galvanically-isolated CAN FD channels**, a 3.3 V UART
service port, and native USB-C for programming via the Arduino IDE or the Tester Present
Windows configurator.

### 1.1 Primary Use Cases
- Inline CAN man-in-the-middle (intercept / modify / inject between vehicle and module)
- Dual-bus gateway / protocol translation (CAN-A ↔ CAN-B, different bit-rates)
- High-rate CAN/CAN-FD data logger with wireless off-load
- ECU emulation / residual-bus simulation on a bench
- Field reflash / configuration tool (talks to a target over CAN via J2534)

### 1.2 Design Pillars (from product brief & concept renders)
1. **Rugged & reliable** — automotive-grade components, sealed enclosure, heavy-duty connector.
2. **Wireless** — Wi-Fi for updates & telemetry, Bluetooth LE for configuration.
3. **Secure & isolated** — galvanic isolation between the two CAN sides for safety.
4. **Easy access** — USB-C programming and diagnostics; Arduino-IDE compatible.

---

## 2. Key Specifications

| Parameter | Specification |
|---|---|
| **MCU** | Espressif ESP32-S3-WROOM-1-N16R8 (dual-core LX7 @ 240 MHz, 16 MB flash, 8 MB PSRAM) |
| **Wireless** | Wi-Fi 802.11 b/g/n 2.4 GHz + Bluetooth 5 LE (on-module PCB antenna) |
| **CAN channels** | 2 × independent, **galvanically isolated** CAN FD (ISO 11898-2) |
| **CAN controller** | 2 × Microchip MCP2518FD (external SPI controllers, 40 MHz) |
| **CAN transceiver** | 2 × TI ISO1042B isolated FD transceiver (±70 V bus fault, 5 Mbit/s) |
| **CAN bit-rates** | Classic CAN 2.0B 125 k–1 Mbit/s; CAN FD arbitration ≤1 Mbit/s, data ≤5 Mbit/s |
| **CAN isolation** | ≥3 kV<sub>RMS</sub> channel-to-logic and channel-to-channel (reinforced option 5 kV<sub>RMS</sub>) |
| **Serial** | 1 × 3.3 V UART (TX/RX) on connector + internal service header |
| **USB** | USB-C 2.0 Full-Speed, native (ESP32-S3 USB-OTG), ESD protected |
| **Programming** | (a) USB-C + Arduino IDE; (b) Tester Present app over USB-serial; (c) over CAN via J2534; (d) over UART serial |
| **Input voltage** | **9–32 V DC nominal** (supports both 12 V and 24 V systems); survives load-dump transients (clamped 53 V) |
| **Input protection** | Fuse + reverse-polarity (ideal-diode) + TVS (ISO 7637-2 pulse tolerant) + π-filter |
| **Quiescent / sleep** | < 200 µA in deep-sleep (ignition-wake), typical run ≈ 0.6–1.4 W |
| **Indicators** | 5 × LED: PWR, Wi-Fi, CAN1, CAN2, STATUS |
| **Connector** | Deutsch **DT** series, **12-way**, sealed (IP67-mated), 13 A/contact |
| **Enclosure** | Sealed black SLA/ABS-like, IP65 target, 4 × heavy-duty mounting ears (brass inserts) |
| **Operating temp** | −40 °C … +85 °C (component-rated); enclosure −20 °C … +70 °C |
| **PCB** | 4-layer FR-4, 1.6 mm, ENIG, isolation slots between CAN domains |
| **Dimensions (PCB)** | ≈ 70 × 50 mm (target) |
| **Dimensions (case)** | ≈ 95 × 70 × 32 mm excl. connector & mounting ears |

---

## 3. System Block Diagram

```
                          ┌───────────────────────────────────────────────┐
                          │              TP-CAN-2I  (Logic Domain)         │
                          │                                                │
   USB-C ──[USBLC6 ESD]───┼──► ESP32-S3-WROOM-1-N16R8 ◄── BOOT/RESET btns  │
   (native USB, Arduino)  │      │  │  │  │   (Wi-Fi + BT antenna on module)│
                          │      │  │  │  └── 5× Status LEDs                │
   UART (3v3) ◄───────────┼──────┘  │  └───── UART1 to connector           │
                          │   SPI2  │                                       │
                          │     ┌───┴────┬─────────┐                        │
                          │     ▼        ▼         │                        │
                          │  MCP2518FD MCP2518FD   │  40 MHz osc (CLKO ⮞)   │
                          │   (CAN1)    (CAN2)      │                        │
                          │     │TXD/RXD  │TXD/RXD                          │
   ═══════════ ISOLATION BARRIER ═══════════════════════════════════════   │
                          │     ▼          ▼                                │
                          │  ISO1042B    ISO1042B   ◄── 2× isolated 5V DCDC │
                          │  (iso xcvr)  (iso xcvr)     (A0505S-1W each)     │
                          │  CMC+TVS+    CMC+TVS+                            │
                          │  term       term                                │
                          └───┬───┬──────┬───┬───────────────────────────────┘
                              │   │      │   │
                          CAN1H CAN1L  CAN2H CAN2L
                         (Vehicle/upstream) (ECU/downstream)
                              │   │      │   │
                          ┌───┴───┴──────┴───┴───────────────────────┐
   9–32V ──[Fuse]──[Ideal │  Deutsch DT 12-way heavy-duty connector  │
   diode rev-prot]──[TVS]─┤  + power + ignition/wake + UART          │
            │             └──────────────────────────────────────────┘
            ▼
      TPS54360B-Q1 (Vin→5V/3.5A, 60V) ──► TLV62569 (5V→3.3V/2A) ──► 3V3 logic
            └──► 5V rail ──► 2× isolated DCDC (CAN side) , USB power-path OR
```

---

## 4. Electrical Architecture

### 4.1 Power Tree (12 V / 24 V capable)

| Stage | Part | Function |
|---|---|---|
| Input fuse | 3 A slow-blow (1206 / Nano2) | over-current / fault isolation |
| Reverse-polarity | LM74700-Q1 ideal-diode ctrl + N-FET (CSD18532) | low-loss reverse protection |
| Transient clamp | SMCJ33A TVS (uni) + π-filter (ferrite + bulk caps) | ISO 7637-2 load-dump / surge |
| Pre-regulator | **TPS54360B-Q1** (4.5–60 V in, 3.5 A) → **5.0 V** | wide-Vin buck, survives 24 V load-dump |
| Logic regulator | **TLV62569** (5 V → **3.3 V**, 2 A) | ESP32-S3 + controllers + logic |
| CAN1 bus supply | **A0505S-1W** isolated DCDC (5 V→5 V iso) | ISO1042B VCC2, isolated domain 1 |
| CAN2 bus supply | **A0505S-1W** isolated DCDC (5 V→5 V iso) | ISO1042B VCC2, isolated domain 2 |
| USB power-path | LM66100 ideal diode (USB 5 V ⟶ 5 V rail) | bench-power from USB without back-feed |

**Rail budget (typical):** 3.3 V @ ≤500 mA (ESP32 Wi-Fi bursts) + controllers/LEDs ≈ 150 mA;
5 V rail feeds 2× 1 W isolated converters (≈ 200 mA each in) + USB. Pre-reg sized 3.5 A for margin.

### 4.2 Microcontroller
- **ESP32-S3-WROOM-1-N16R8**: native USB-OTG (no USB-UART bridge required), dual-core,
  16 MB flash / 8 MB PSRAM for large MITM buffers and OTA dual-bank.
- Octal-PSRAM variant occupies GPIO33–37 internally — **those GPIO are reserved, do not use.**
- Antenna keep-out per module datasheet must overhang the PCB edge with no copper beneath.

### 4.3 Dual CAN FD (Isolated)
- Two **MCP2518FD** SPI CAN-FD controllers share **SPI2 (FSPI)** with independent CS + INT.
- One **40 MHz** oscillator clocks controller 1; its **CLKO** output daisy-chains controller 2.
- Each controller's TXD/RXD crosses the isolation barrier through an **ISO1042B** transceiver.
- Each transceiver bus side is powered by its **own isolated 5 V** supply → channel-to-channel
  **and** channel-to-logic galvanic isolation (true MITM safety).
- Bus protection per channel: **common-mode choke** (TDK ACT45B) + **CAN TVS** (PESD2CAN)
  + **split termination** (2 × 60 Ω + 4.7 nF to isolated bus GND) + jumper-selectable 120 Ω.

### 4.4 USB / Programming
- USB-C receptacle (16-pin, with mounting tabs), **CC1/CC2 5.1 kΩ** pull-downs (UFP).
- **USBLC6-2SC6** ESD array on D+/D−.
- Native USB connects to ESP32-S3 GPIO19 (D−) / GPIO20 (D+).
- **BOOT (GPIO0)** + **RESET (EN)** tactiles for manual flashing; auto-reset supported by native USB.

### 4.5 Serial UART
- **UART1** (GPIO17 TX / GPIO18 RX), 3.3 V logic, brought to both the connector and an
  internal 4-pin service header (TX, RX, GND, 3V3).

### 4.6 Status Indicators
| LED | Colour | Driven by | Meaning |
|---|---|---|---|
| PWR | Green | Hard-wired to 3V3 | Power good |
| Wi-Fi | Blue | GPIO38 | Wi-Fi/BT activity |
| CAN1 | Amber | GPIO39 | Vehicle-side bus traffic |
| CAN2 | Amber | GPIO40 | ECU-side bus traffic |
| STATUS | Blue | GPIO41 | System/MITM state (heartbeat/error codes) |

---

## 5. ESP32-S3 Pin Allocation

| Function | GPIO | Notes |
|---|---|---|
| USB D− | 19 | native USB (fixed) |
| USB D+ | 20 | native USB (fixed) |
| SPI2 SCK | 12 | to both MCP2518FD |
| SPI2 MOSI (SDI) | 11 | to both MCP2518FD |
| SPI2 MISO (SDO) | 13 | from both MCP2518FD |
| CAN1 CS | 10 | MCP2518FD #1 chip-select |
| CAN2 CS | 14 | MCP2518FD #2 chip-select |
| CAN1 INT | 4 | MCP2518FD #1 interrupt (input) |
| CAN2 INT | 5 | MCP2518FD #2 interrupt (input) |
| UART1 TX | 17 | 3.3 V serial to connector/header |
| UART1 RX | 18 | 3.3 V serial to connector/header |
| LED Wi-Fi | 38 | |
| LED CAN1 | 39 | |
| LED CAN2 | 40 | |
| LED STATUS | 41 | |
| BOOT | 0 | strapping — tactile to GND |
| Ignition/Wake sense | 16 | via divider+TVS; RTC-capable for deep-sleep wake |
| **Reserved** | 26–37 | SPI flash + **octal PSRAM** (N16R8) — DO NOT USE |
| **Avoid as outputs** | 0, 3, 45, 46 | boot strapping pins |

---

## 6. Heavy-Duty Connector — Deutsch DT 12-Way

External harness connector: **Deutsch DT** series, 12-way, sealed (IP67 mated), 13 A/contact,
14–16 AWG capable. Enclosure-wall flange receptacle → short internal loom → polarised locking
board header (Molex Micro-Fit 3.0, 12-ckt) on the PCB ("ECU-style" serviceable interface).

| Pin | Signal | Domain | Wire (AWG / colour) |
|---|---|---|---|
| 1 | +VBAT (9–32 V power in) | Power | 16 / Red |
| 2 | GND (power / chassis) | Power | 16 / Black |
| 3 | IGN / WAKE (switched +) | Logic | 18 / White |
| 4 | CAN1-H (Vehicle / upstream) | Iso domain 1 | 18 / Yellow (twisted pair) |
| 5 | CAN1-L (Vehicle / upstream) | Iso domain 1 | 18 / Green (twisted pair) |
| 6 | CAN1-GND (vehicle bus ref/shield) | Iso domain 1 | 18 / Bare/Drain |
| 7 | CAN2-H (ECU / downstream) | Iso domain 2 | 18 / Orange (twisted pair) |
| 8 | CAN2-L (ECU / downstream) | Iso domain 2 | 18 / Blue (twisted pair) |
| 9 | CAN2-GND (ECU bus ref/shield) | Iso domain 2 | 18 / Bare/Drain |
| 10 | UART-TX (3.3 V out) | Logic | 22 / Grey |
| 11 | UART-RX (3.3 V in) | Logic | 22 / Violet |
| 12 | UART-GND (logic ref) | Logic | 22 / Brown |

> **Note:** CAN-GND pins (6, 9) reference each isolated transceiver's bus-side ground.
> For a true MITM in a single-chassis vehicle they may be left unterminated or tied to
> local chassis at each node; on a bench they provide the isolated return.

Connector part numbers and the harness build procedure are in
[`/Harness/Connector_and_Harness.md`](../Harness/Connector_and_Harness.md).

---

## 7. Isolation Strategy

- **Logic domain:** ESP32-S3, both MCP2518FD controllers, USB, power regulators, LEDs.
- **Isolated domain 1 (CAN1):** ISO1042B bus side + its isolated 5 V supply.
- **Isolated domain 2 (CAN2):** ISO1042B bus side + its isolated 5 V supply.
- Barrier carried by the ISO1042B reinforced isolation and the isolated DCDC transformers.
- **PCB:** isolation **slots milled** under each ISO1042B and DCDC, ≥ 4 mm creepage logic-to-bus
  and bus-to-bus; no copper, no silkscreen, no traces crossing the barrier except through the
  rated parts. Maintain ≥ 8 mm spacing between the two CAN domains where practical.

---

## 8. Mechanical Envelope

- Two-part sealed enclosure (base + lid) with perimeter **gasket groove** (O-ring cord or
  cured-in-place gasket), IP65 target.
- **4 heavy-duty mounting ears** with **brass heat-set inserts** / steel eyelets for M5 bolts.
- Deutsch DT flange receptacle on one end face; **sealed USB-C service port** (rubber bung)
  on an adjacent face.
- PCB on **4 standoffs/bosses** with M3 brass inserts; 1.5 mm clearance to lid for components.
- **LED light-pipes** (5) through the lid, or a clear window strip.
- Full model + drawing: [`/Enclosure/`](../Enclosure/).

---

## 9. Compliance & Environment (targets)

| Item | Target |
|---|---|
| Conducted transients | ISO 7637-2 pulses 1, 2a, 2b, 3a, 3b (clamped front-end) |
| Supply ranges | ISO 16750-2 (12 V & 24 V systems), load-dump survival |
| ESD | IEC 61000-4-2 ±8 kV contact on exposed I/O & USB |
| CAN PHY | ISO 11898-2:2016 (CAN FD) |
| Ingress | IP65 (enclosure), IP67 (mated connector) |
| RoHS / REACH | Compliant component selection |

---

## 10. Programming & Configuration Interfaces

| Path | Transport | Use | Tool |
|---|---|---|---|
| Firmware flash | USB-C native | Load/replace firmware | **Arduino IDE** (ESP32-S3 board) |
| Firmware flash | USB-C native | Load/replace firmware | Tester Present app (esptool backend) |
| Configuration | USB-serial (CDC) | Set CAN bit-rates, MITM rules, filters, Wi-Fi creds | Tester Present app |
| Configuration | **CAN via J2534** | Program/config over the bus | Tester Present app (J2534 PassThru) |
| Configuration | UART serial | Headless config | Tester Present app / terminal |
| Live / OTA | Wi-Fi | Telemetry, OTA update | Tester Present app / web |
| Config | Bluetooth LE | Mobile/field config | (future) |

The on-the-wire configuration protocol is defined in the firmware and mirrored by the
Windows app — see [`/Firmware`](../Firmware/) and [`/Software`](../Software/).

---

## 11. Deliverables Map

| Folder | Contents |
|---|---|
| `/Docs` | This spec, datasheet, design notes |
| `/Hardware/KiCad` | KiCad project (schematic, PCB, netlist) |
| `/Hardware` | `BOM.csv`, fabrication notes |
| `/Enclosure` | OpenSCAD case (base + lid), STL output, drawing |
| `/Harness` | Deutsch DT pinout + harness build document |
| `/Firmware` | ESP32-S3 Arduino firmware |
| `/Software` | .NET 4.8.1 WinForms configurator (J2534 + serial) |

---
*Tester Present Specialist Automotive Solutions — Designed in Australia — © 2026 Jack Leighton.*
