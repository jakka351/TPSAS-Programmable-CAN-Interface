# TP-CAN-2I — PCB Fabrication & Assembly Notes

## 1. Board Summary
| Item | Value |
|---|---|
| Size | 70.0 × 50.0 mm, 3 mm corner radius |
| Layers | 4 (signal / GND / PWR / signal) |
| Thickness | 1.6 mm |
| Material | FR-4, Tg ≥ 150 °C (automotive temp) |
| Copper | 1 oz outer, 0.5 oz inner (1 oz inner if budget allows) |
| Finish | ENIG (gold, for fine-pitch ESP32 + ISO parts) |
| Soldermask | Matte black |
| Silkscreen | White |
| Min track/space | 0.2 mm / 0.2 mm (design uses ≥0.25 mm) |
| Min drill | 0.3 mm |
| Mounting holes | 4 × Ø3.2 mm NPTH (M3) |
| Special | 2 × routed isolation slots; board-edge antenna keep-out |

## 2. Stackup (target — confirm with fab)
```
 L1  F.Cu      35 µm   signal + components (top)
     prepreg  ~0.21 mm
 L2  In1.Cu    17 µm   GND plane  (continuous reference)
     core     ~1.065 mm
 L3  In2.Cu    17 µm   PWR plane  (+3V3 / +5V pours)
     prepreg  ~0.21 mm
 L4  B.Cu      35 µm   signal + ground pour (bottom)
 Total ≈ 1.6 mm
```
Controlled impedance is not required (CAN is low-speed differential, well below the need
for tight Zdiff), but keep CAN H/L as a tightly-coupled pair with matched length.

## 3. Net Classes / Design Rules
Defined in `TP-CAN-2I.kicad_pro` and `TP-CAN-2I.kicad_dru`:
| Class | Clearance | Track | Use |
|---|---|---|---|
| Default | 0.20 mm | 0.25 mm | signals |
| Power | 0.30 mm | 0.60 mm | VBAT/+5V/+3V3 |
| CAN_LOGIC | 0.25 mm | 0.30 mm | controller↔isolator TXD/RXD |
| CAN1_ISO | 0.30 mm | 0.40 mm | vehicle bus (isolated domain 1) |
| CAN2_ISO | 0.30 mm | 0.40 mm | ECU bus (isolated domain 2) |

**Isolation rule (`.kicad_dru`):** ≥ **4 mm** creepage between each `*_ISO` domain and every
other domain (logic and the other bus). The two milled slots under U9/U10 increase creepage
locally. Do **not** pour copper, place vias, or route traces across the barrier.

## 4. Layer Routing Plan
- **L1 (F.Cu):** components + short signal routing; CAN diff pairs; SPI bus.
- **L2 (GND):** solid ground plane. Split only where the isolation barrier crosses — the
  isolated bus domains get their **own** local ground pours (GND_ISO1, GND_ISO2) on L1/L4,
  NOT connected to L2 GND except through ISO parts.
- **L3 (PWR):** +3V3 pour (logic 2/3) and +5V pour; isolated +5V_ISOn pours are local islands
  near each transceiver only.
- **L4 (B.Cu):** ground pour + overflow routing; keep CAN pairs together if routed here.
- **Antenna:** the ESP32-S3-WROOM-1 PCB-antenna end overhangs the top board edge; **no copper
  on any layer** under the keep-out (see module datasheet, ~15 × 6 mm).

## 5. Critical Layout Rules
1. Place 100 nF decoupling at every IC VDD pin, vias straight to plane.
2. ESP32-S3: bulk 4.7 µF + 100 nF close; thermal vias under module GND pad.
3. TPS54360 buck: tight hot loop (VIN cap → SW → catch diode → GND); keep SW node small;
   FB trace away from SW/inductor; thermal pad → GND with via array.
4. CAN: H/L tightly coupled; common-mode choke close to connector; split-termination + TVS
   right at the bus entry; isolated ground pour referenced to each transceiver's GND2.
5. Each isolated DC-DC (A0505S): keep primary/secondary copper separated under the slot/keepout.
6. USB-C: D± as 90 Ω diff pair, short, ESD diode (USBLC6) at the connector.

## 6. Fabrication Output Checklist (export from KiCad once routed)
- [ ] Gerber X2 (all copper, mask, silk, paste, Edge.Cuts) — `/Hardware/Production`
- [ ] Excellon drill (PTH + NPTH, with map)
- [ ] IPC-356 netlist (for bare-board electrical test)
- [ ] Pick-and-place (CPL) top/bottom
- [ ] BOM (`/Hardware/BOM.csv`)
- [ ] PDF assembly drawing + fab drawing (stackup, notes)
- [ ] 3D STEP (for enclosure fit-check against `/Enclosure`)

## 7. Assembly Notes
- Single-sided SMT (all parts top) to reduce cost; through-hole only for the DC-DC modules,
  pin headers, and connector if a TH variant is chosen.
- Reflow profile per ENIG + ESP32 module (Espressif reflow guideline, peak ≤ 245 °C).
- Conformal coat after test (automotive humidity/vibration) — mask the connector & USB-C.
