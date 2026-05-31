# TP-CAN-2I — Schematic Design (Net-by-Net)

Connections are given by **pin name** (unambiguous for schematic capture). Passive feedback /
timing values are nominal starting points — **confirm against the latest device datasheet**
during schematic finalisation. Reference designators match [`BOM.csv`](BOM.csv).

**Net domains**
- **Logic GND** — ESP32-S3, controllers, regulators, USB.
- **GND_ISO1 / +5V_ISO1** — isolated bus domain for CAN1 (vehicle/upstream).
- **GND_ISO2 / +5V_ISO2** — isolated bus domain for CAN2 (ECU/downstream).
- The three grounds connect **only** through the isolation parts (ISO1042B, A0505S).

---

## Block 1 — Power Input & Protection

```
DT pin1 +VBAT ─► F1(3A) ─► [Q1 N-FET + U11 LM74700 ideal-diode] ─► VBAT_PROT
DT pin2 GND   ─────────────────────────────────────────────────► Logic GND
```
- **F1** in series with +VBAT from connector pin 1.
- **U11 LM74700-Q1** ideal-diode controller drives **Q1 (CSD18532 N-FET)** as reverse-polarity
  pass element: VBAT(fused) → Q1 drain; Q1 source → VBAT_PROT. U11 VS=source, VIN(anode)=drain,
  GATE=Q1 gate, GND=Logic GND. (Blocks reverse current, ~ideal forward drop.)
- **D2 SMCJ33A** TVS: VBAT_PROT → Logic GND (clamps load-dump to ≤53 V).
- **L1 (4.7 µH)** + **C1 (100 µF)** + **C2 (2×10 µF)**: π input filter on VBAT_PROT.

## Block 2 — 5 V Pre-Regulator (U1 TPS54360B-Q1)

| Pin | Net | Notes |
|---|---|---|
| VIN | VBAT_PROT | + C2 bypass |
| EN | VBAT_PROT via R12(499k)/R13(75k) divider | programmable UVLO (~6–7 V on) |
| BOOT | → 100 nF → SW | bootstrap cap |
| SW | → L2(22 µH) → +5V; **D6 SS34** cathode→SW, anode→GND | catch diode (non-sync) |
| FB | +5V via R9(52.3k)/R10(10k) to GND | sets 5.0 V (Vref 0.8 V) |
| RT/CLK | → R11(182k) → GND | ~500 kHz |
| COMP | → R-C compensation → GND | per datasheet (e.g. 3.3 nF + 13 k) |
| GND / PAD | Logic GND | thermal pad to GND pour |

Output **+5V** bulk: **C4 47 µF** + 100 nF. Clamp **D5 SMAJ5.0A** on +5V.

## Block 3 — 3.3 V Logic Regulator (U2 TLV62569)

| Pin (name) | Net |
|---|---|
| VIN | +5V |
| EN | +5V (or GPIO for sequencing) |
| SW | → L3(2.2 µH) → +3V3 |
| FB | +3V3 via R14(540k)/R15(120k) to GND (3.3 V, Vref 0.6 V) |
| GND | Logic GND |

Output **+3V3** bulk: **C5 22 µF** + 100 nF. Powers ESP32-S3, both MCP2518FD (VDD),
ISO1042B **logic side** (VCC1), oscillator, LEDs.

## Block 4 — Isolated CAN Bus Supplies (U3, U4 A0505S-1W)

Two independent modules, each: **+5V → +5V_ISOn**.
- U3: VIN=+5V, GND=Logic GND, VOUT=+5V_ISO1, 0V=GND_ISO1. Bulk **C9 10 µF** on +5V_ISO1.
- U4: VIN=+5V, GND=Logic GND, VOUT=+5V_ISO2, 0V=GND_ISO2. Bulk **C9 10 µF** on +5V_ISO2.
- Input bypass 4.7 µF each. Provides galvanic isolation for each transceiver bus side.

## Block 5 — USB-C Power-Path (U5 LM66100)

- USB **VBUS** → U5 (ideal diode) → +5V rail (OR-ing with buck output so either source powers
  logic on the bench without back-feeding the vehicle supply).
- U5 ON pin tied for auto-on; current-limit per datasheet.

---

## Block 6 — MCU (U6 ESP32-S3-WROOM-1-N16R8)

| Module pin | Net | Notes |
|---|---|---|
| 3V3 | +3V3 | + C10 4.7 µF + 100 nF |
| GND (all) | Logic GND | thermal/RF ground |
| EN | +3V3 via R5(10k); SW2 to GND; 1 µF to GND | reset, RC delay |
| IO0 | R5(10k) to +3V3; SW1 to GND | BOOT strap |
| IO19 | USB_DM | native USB D− |
| IO20 | USB_DP | native USB D+ |
| IO12 | SPI_SCK | to both MCP2518FD SCK |
| IO11 | SPI_MOSI | to both MCP2518FD SDI |
| IO13 | SPI_MISO | from both MCP2518FD SDO |
| IO10 | CAN1_CS | MCP2518FD #1 nCS (R6 10k pull-up to 3V3) |
| IO14 | CAN2_CS | MCP2518FD #2 nCS (R6 10k pull-up to 3V3) |
| IO4 | CAN1_INT | MCP2518FD #1 nINT |
| IO5 | CAN2_INT | MCP2518FD #2 nINT |
| IO17 | UART1_TX | to connector pin10 + service header |
| IO18 | UART1_RX | from connector pin11 + service header |
| IO16 | IGN_SENSE | from divider R2(100k)/R3(22k) off DT pin3 + TVS |
| IO38 | LED_WIFI | → R4(1k) → LED2(blue) → GND |
| IO39 | LED_CAN1 | → R4(1k) → LED3(amber) → GND |
| IO40 | LED_CAN2 | → R4(1k) → LED4(amber) → GND |
| IO41 | LED_STATUS | → R4(1k) → LED5(blue) → GND |
| (3V3) | LED_PWR | +3V3 → R4(1k) → LED1(green) → GND (hard-wired) |

**Reserved (do not connect):** IO26–IO37 (flash + octal PSRAM). Avoid driving IO0/IO3/IO45/IO46.

## Block 7 — USB-C (J1) + ESD (D1)

| J1 | Net |
|---|---|
| VBUS | USB VBUS → U5 power-path |
| GND / SHIELD | Logic GND (shield via RC to GND) |
| CC1 | R1(5.1k) → GND |
| CC2 | R1(5.1k) → GND |
| D+ (both) | → D1 USBLC6 → USB_DP → IO20 |
| D− (both) | → D1 USBLC6 → USB_DM → IO19 |

**D1 USBLC6-2SC6**: I/O1=D+, I/O2=D−, VBUS pin→USB VBUS, GND→GND.

---

## Block 8 — CAN Controllers (U7, U8 MCP2518FD)

Shared SPI bus, independent CS/INT. **Clock:** Y1 40 MHz CMOS oscillator → U7 OSC1; U7
**OSC2/CLKO** → U8 OSC1 (daisy-chain). Each VDD=+3V3 + 100 nF; VSS=Logic GND.

| Signal | U7 (CAN1) | U8 (CAN2) |
|---|---|---|
| nCS | CAN1_CS (IO10) | CAN2_CS (IO14) |
| SCK | SPI_SCK | SPI_SCK |
| SDI | SPI_MOSI | SPI_MOSI |
| SDO | SPI_MISO | SPI_MISO |
| nINT | CAN1_INT (IO4) | CAN2_INT (IO5) |
| OSC1 | Y1 40 MHz | U7 CLKO |
| TXCAN | CAN1_TXD → U9 TXD | CAN2_TXD → U10 TXD |
| RXCAN | CAN1_RXD ← U9 RXD | CAN2_RXD ← U10 RXD |
| VDD/VSS | +3V3 / GND | +3V3 / GND |

## Block 9 — Isolated Transceivers (U9, U10 ISO1042B) + Protection

**U9 = CAN1 (vehicle), U10 = CAN2 (ECU).** Logic side from +3V3/Logic GND; bus side from
+5V_ISOn / GND_ISOn.

| ISO1042B pin | CAN1 (U9) | CAN2 (U10) |
|---|---|---|
| VCC1 (logic) | +3V3 + 100 nF | +3V3 + 100 nF |
| GND1 | Logic GND | Logic GND |
| TXD | CAN1_TXD | CAN2_TXD |
| RXD | CAN1_RXD | CAN2_RXD |
| VCC2 (bus) | +5V_ISO1 + 100 nF + C9 10 µF | +5V_ISO2 + … |
| GND2 | GND_ISO1 | GND_ISO2 |
| CANH | → CMC1 → CAN1H_raw | → CMC2 → CAN2H_raw |
| CANL | → CMC1 → CAN1L_raw | → CMC2 → CAN2L_raw |

**Per-channel bus network (after the common-mode choke CMCn):**
- **Split termination:** R7(60 Ω) CANH→TERMn, R7(60 Ω) TERMn→CANL, **C7 4.7 nF** TERMn→GND_ISOn.
- **Selectable 120 Ω:** R8(120 Ω) across CANH/CANL via jumper J4 (CAN1) / J5 (CAN2).
- **TVS:** D3/D4 **PESD2CAN** CANH/CANL → GND_ISOn.
- CANH/CANL → connector: CAN1→DT pins 4/5, CAN2→DT pins 7/8. GND_ISO1→pin6, GND_ISO2→pin9.

---

## Block 10 — Connectors & Headers

- **J2 Micro-Fit 12-ckt** (internal board header → harness loom): maps 1:1 to the DT 12-way
  pinout in the spec (power, IGN, CAN1 H/L/GND, CAN2 H/L/GND, UART TX/RX/GND).
- **J3 UART service header (1×4):** +3V3, UART1_TX, UART1_RX, Logic GND.
- **J4 / J5:** CAN1 / CAN2 termination select (center = bus, ends = enable 120 Ω / off).

## Block 11 — Ignition / Wake

- DT pin3 (switched +) → R2(100k) → IGN_SENSE node → R3(22k) → GND; TVS to GND; 100 nF.
  IGN_SENSE → ESP32 IO16 (RTC-capable) for deep-sleep wake on ignition.

---

## Net Summary (key)

| Net | Members |
|---|---|
| VBAT_PROT | F1, Q1, U11, D2, L1, C1/C2, U1.VIN |
| +5V | U1 out, U2.VIN, U3.VIN, U4.VIN, U5 out, C4 |
| +3V3 | U2 out, U6.3V3, U7/U8.VDD, U9/U10.VCC1, Y1, LEDs, C5/C10 |
| +5V_ISO1 / GND_ISO1 | U3 out, U9.VCC2/GND2, C9 |
| +5V_ISO2 / GND_ISO2 | U4 out, U10.VCC2/GND2, C9 |
| SPI_SCK/MOSI/MISO | U6 ↔ U7,U8 |
| CAN1_TXD/RXD | U7 ↔ U9 |
| CAN2_TXD/RXD | U8 ↔ U10 |
| CAN1H/L | U9 → CMC1 → term/TVS → DT 4/5 |
| CAN2H/L | U10 → CMC2 → term/TVS → DT 7/8 |

> Isolation barrier runs between {GND1/VCC1/TXD/RXD} and {GND2/VCC2/CANH/CANL} of each
> ISO1042B, and across each A0505S. PCB: route **no** copper across the barrier; mill slots.
