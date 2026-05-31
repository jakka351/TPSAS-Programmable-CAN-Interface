# TP-CAN-2I — Heavy-Duty Connector & Wiring Harness

**Model:** TP-CAN-2I Programmable Dual-CAN Inline Interface · **Rev A**
© 2026 Jack Leighton — Tester Present Specialist Automotive Solutions · Designed in Australia

This document specifies the sealed automotive connector on the device, the internal board
interface, the device-side service harness, and the recommended in-vehicle (inline / pass-through)
deployment. It is written so a harness shop can build it and an installer can fit it.

---

## 1. Connector System Overview

The device presents **one sealed 12-way Deutsch DT receptacle** on the +X end wall. A single
mating plug carries everything the device needs: wide-range power, ignition sense, **both isolated
CAN buses**, and the **UART service link**.

```
            DEVICE (sealed enclosure)                         FIELD HARNESS
   ┌───────────────────────────────────┐
   │  PCB                               │        DT04-12PA            DT06-12SA
   │  ┌──────────┐   internal pigtail   │        receptacle          plug (mates)
   │  │ J2 Molex │ 12× size-16  ┌───────┴──┐    (pins, device)      (sockets, harness)
   │  │ Micro-Fit│══════════════│ bulkhead │◄════════╗
   │  │ 3.0 12ck │   150 mm      │ DT recep │         ║  ──► Power branch (FUSED)
   │  └──────────┘              └───────┬──┘         ║  ──► Vehicle branch  (CAN1)
   │                                    │            ║  ──► ECU branch      (CAN2)
   └───────────────────────────────────┘            ║  ──► UART service pigtail
                                                  trunk ~300 mm
```

Two-stage interface — **the same approach a production ECU uses**:

| Stage | Part | Why |
|---|---|---|
| **External** (case wall ↔ field harness) | Deutsch **DT 12-way**, sealed IP67 | Rugged, vibration-proof, glove-friendly, industry standard for off-road/truck/marine |
| **Internal** (case wall ↔ PCB) | Molex **Micro-Fit 3.0 12-ckt** + short pigtail | Lets the lid/PCB be serviced without disturbing the sealed bulkhead; polarised, latched |

> The DT family is a *wire-to-wire* system; it has no PCB-mount variant by design. Bridging it to
> the board with a short internal pigtail to a Micro-Fit header is the correct, field-proven pattern
> and keeps the IP67 bulkhead seal independent of board service.

---

## 2. Device-Side Connector — Deutsch DT 12-Way (Bill of Material)

All parts TE Connectivity / Deutsch. Quantities are **per device** unless noted.

| Item | TE / Deutsch P/N | Qty | Notes |
|---|---|---|---|
| Receptacle, 12-way, **A**-key (holds **pins**) | **DT04-12PA** | 1 | Mounts in case wall; device side |
| Wedgelock for receptacle | **W12P** | 1 | Secondary lock for DT04-12PA |
| Solid **pin** contacts, size 16 (nickel) | **0460-202-16141** | 12 | 16–20 AWG, 13 A; gold = 0460-202-1631 |
| Cavity sealing plugs (size 16) | **0413-204-1605** | as needed | Fill any unused cavity to keep IP67 |
| **Mating** plug, 12-way, A-key (holds **sockets**) | **DT06-12SA** | 1 | Harness side |
| Wedgelock for plug | **W12S** | 1 | Secondary lock for DT06-12SA |
| Solid **socket** contacts, size 16 (nickel) | **0462-201-16141** | 12 | 16–20 AWG; gold = 0462-201-1631 |

**Tooling**
- Crimp (size-16 solid contacts): Deutsch **HDT-48-00** hand crimper (or equivalent 4-indent).
- Contact removal: size-16 release tool **0411-336-1605**.
- Wedgelock seating: by hand until it clicks; never force past the lock tabs.

**Keying.** Both halves specified **A-key** so the harness can only seat one way and will not
mate with another DT-12 on the vehicle. If the same vehicle already runs DT-12 connectors, order
the device with **B** or **C** key (factory option) to prevent cross-mating.

---

## 3. Bulkhead Mounting (case wall)

The enclosure (`/Enclosure/TP-CAN-2I_enclosure.scad`) provides the matching cutout and bolt
pattern on the +X wall:

- Aperture: **25.0 mm (Y) × 21.0 mm (Z)**, lower edge **+6 mm above the inner floor**.
- Two flange screws: **M3, 31.0 mm spacing (Y)**, centred on the aperture.

Retain the DT04-12PA by clamping its rear shoulder to the inside wall with a **1.5 mm stainless
retention plate** (laser-cut, bolt pattern above) — this is a fabricated part the case shop makes
from the enclosure drawing. The receptacle's integral interfacial seal + the wall face give the
IP67 line; add a thin smear of dielectric grease on the seal at assembly. (TE's snap-in panel
clip may be substituted if the wall is left at nominal 3 mm.)

---

## 4. Internal Board Interface — Molex Micro-Fit 3.0 (12-circuit)

| Item | Molex P/N | Qty | Notes |
|---|---|---|---|
| PCB header, dual-row 12-ckt, 3.00 mm | **0430451200** (43045-1200) | 1 | Ref **J2**, on PCB |
| Crimp receptacle housing, 12-ckt | **0430251200** (43025-1200) | 1 | On internal pigtail |
| Female crimp terminals (20–24 AWG) | **0430300007** (43030-0007) | 12 | Gold; tin = 43030-0001 |

**Internal pigtail.** 12 leads, **150 mm**, size-16 *solid pin* contacts on the bulkhead end
(into DT04-12PA) and Micro-Fit terminals on the J2 end. Use **20 AWG TXL** for all 12 internal
leads (fits both contact systems; the run is short so power loss is negligible). Dress the pigtail
with a service loop so the lid lifts ~40 mm without strain.

---

## 5. Master Pinout (12-way) — applies to DT, the pigtail, and J2

Pin numbering follows the **DT04-12PA cavity map** (stamped on the receptacle face). The internal
pigtail maps DT cavity *n* → J2 pin *n* one-to-one.

| Pin | Signal | Domain | Direction | Wire (field harness) | Colour |
|---:|---|---|---|---|---|
| 1 | **+VBAT** (9–32 V) | Power | IN | 16 AWG | Red |
| 2 | **GND** (power return) | Power | — | 16 AWG | Black |
| 3 | **IGN / WAKE** (switched +12 V sense) | Power | IN | 20 AWG | Pink |
| 4 | **CAN1_H** (vehicle bus) | CAN1 ISO | bidir | 18 AWG, twisted w/ 5 | Yellow |
| 5 | **CAN1_L** (vehicle bus) | CAN1 ISO | bidir | 18 AWG, twisted w/ 4 | Green |
| 6 | **CAN1_GND / shield** | CAN1 ISO | — | 20 AWG drain | Bare/Blk-Wht |
| 7 | **CAN2_H** (ECU bus) | CAN2 ISO | bidir | 18 AWG, twisted w/ 8 | Wht/Yellow |
| 8 | **CAN2_L** (ECU bus) | CAN2 ISO | bidir | 18 AWG, twisted w/ 7 | Wht/Green |
| 9 | **CAN2_GND / shield** | CAN2 ISO | — | 20 AWG drain | Bare/Blk-Wht |
| 10 | **UART_TX** (device → host) | Logic | OUT | 20 AWG | Blue |
| 11 | **UART_RX** (host → device) | Logic | IN | 20 AWG | Orange |
| 12 | **UART_GND** | Logic | — | 20 AWG | Brown |

> **Isolation note.** Pins 4–6 (CAN1) and 7–9 (CAN2) are **galvanically isolated** from each other
> and from logic/power inside the device. *Do not bond* CAN1_GND, CAN2_GND, UART_GND, or power GND
> together anywhere in the harness — each ground stays with its own twisted set. Joining them
> defeats the isolation barrier that protects the vehicle and ECU.

---

## 6. Wire Specification

- **Power (1,2):** 16 AWG TXL/GXL automotive, 105 °C. Carries device draw (≤0.6 A typ.) plus margin.
- **Ignition (3):** 20 AWG TXL. High-impedance sense input; current is negligible.
- **CAN pairs (4/5, 7/8):** 18 AWG, **120 Ω shielded twisted pair**, ~33–40 twists/m. Keep each pair
  twisted to within 30 mm of the contact. Drain wire (6, 9) bonded to the foil shield **at the
  device end only** (avoid ground loops).
- **UART (10,11,12):** 20 AWG; run as a loose triad. Optional foil shield for noisy installs,
  drained to UART_GND at the device end.
- All contacts are **size-16 (16–20 AWG range)** — every wire above is inside that window.

---

## 7. Field Harness Construction (device-side trunk + branches)

The supplied harness is a **Y (fan-out)**: one DT06-12SA plug on a ~300 mm trunk, breaking out to
four branches. Trunk and branches run in **Ø10 mm split convoluted tubing**, taped at the breakout.

```
  DT06-12SA ──┬─[POWER]──── 7.5 A ATO fuse ── Red 16AWG ──► Battery + (ring, M6)
   (to device)│            └─────────────────  Black 16AWG ─► Chassis GND (ring, M6)
              │            └─ Pink 20AWG ─────────────────► Switched-IGN tap
              │
              ├─[VEHICLE / CAN1]── STP ─► Vehicle bus  (app-specific term., built to order)
              │
              ├─[ECU / CAN2]────── STP ─► ECU/module   (app-specific term., built to order)
              │
              └─[SERVICE / UART]── triad ─► sealed 4-way service connector (capped when unused)
```

**Build steps**
1. Cut trunk leads to length; maintain CAN twist all the way into the breakout.
2. Strip 5–6 mm; crimp size-16 **sockets** (0462-201-16141) with HDT-48-00. Inspect: insulation in
   the rear barrel, conductor visible in the inspection hole, no stray strands.
3. Insert each contact into its DT06-12SA cavity per §5 until it clicks (gentle tug test). Seat the
   **W12S** wedgelock; it will not seat fully if any contact is short — that's the built-in check.
4. Fill unused cavities (if a reduced build) with **0413-204-1605** plugs.
5. **Power branch:** inline **7.5 A** ATO fuse within 150 mm of the battery ring. Heat-shrink all
   joints with **adhesive-lined** tubing. IGN to a switched +12 V source that is dead with the key off.
6. **CAN branches:** keep shields isolated from chassis at the vehicle/ECU end; drain only at the device.
7. **Service branch:** terminate in a small sealed connector (e.g. 4-way DT or AMP Superseal) for
   UART_TX/RX/GND + a spare; fit the cap when not in use to preserve sealing.
8. Strain-relieve the trunk to the device with the DT plug's own latch; add a P-clip within 100 mm.
9. Label each branch (POWER / VEHICLE-CAN1 / ECU-CAN2 / SERVICE).

---

## 8. Recommended Deployment — true inline (pass-through)

The device is a **man-in-the-middle**: it sits *between the vehicle harness and the ECU* so it can
monitor, pass, filter, or rewrite traffic. The cleanest fit does **not cut** vehicle wiring:

```
   VEHICLE HARNESS ──►│ Vehicle branch (CAN1) │  TP-CAN-2I  │ ECU branch (CAN2) │──► ECU / MODULE
   (existing plug)     mates vehicle-side conn               mates ECU-side conn
```

- **Vehicle branch (CAN1)** is built with the **mating half of the vehicle's existing ECU
  connector**, so the vehicle harness that *used* to plug into the ECU now plugs into the device.
- **ECU branch (CAN2)** is built with **the ECU's own connector type**, so the device plugs into
  the ECU in the harness's place.
- Result: the device is electrically in series on the bus, fully isolated each side, and removable
  by unplugging two connectors — no splicing, factory-reversible.

Because vehicle/ECU connectors differ per platform, **those two end terminations are built to order**
per application. Provide the harness shop with the vehicle and ECU connector P/Ns; the device-side
trunk in §7 is fixed and never changes.

**Bus termination.** Each CAN channel has an on-board, firmware-selectable **120 Ω split
termination** (see `/Hardware/Schematic_Design.md` and `/Hardware/Fabrication_Notes.md`). When the
device replaces an end-of-bus node, enable termination on that channel; when it taps mid-bus, leave
it disabled. **The harness adds no termination resistors.**

---

## 9. Continuity / Acceptance Test (every harness)

Before shipping, verify on the assembled trunk (DT06-12SA, contacts seated, wedgelock home):

| Check | Method | Pass |
|---|---|---|
| Pin-out 1:1 | DMM continuity, DT pin *n* → branch lead *n* | < 0.3 Ω each |
| No adjacent shorts | DMM, every pin to its neighbours | > 20 MΩ |
| CAN pair integrity | 4↔5 and 7↔8 not shorted; twist intact | > 20 MΩ, visual |
| Ground isolation | 2 ↔ 6 ↔ 9 ↔ 12 mutually open | > 20 MΩ (proves isolation kept) |
| Fuse present & rated | Visual, power branch | 7.5 A ATO |
| Wedgelocks seated | Visual + tug each contact | No back-out |
| Seal integrity | DT plug fully latched; service cap fitted | Click + visual |

Record results on the build traveller. A failed **ground-isolation** check is a reject — it means a
return wire was cross-bonded and the device's isolation would be defeated.

---

## 10. Quick-Reference Card (laminate for installers)

```
TP-CAN-2I  DT-12 PINOUT          POWER: Pin1 +VBAT(9-32V)  Pin2 GND  Pin3 IGN
 1 +VBAT  2 GND   3 IGN          CAN1 (VEHICLE): Pin4 H  Pin5 L  Pin6 shield
 4 CAN1H  5 CAN1L 6 CAN1GND      CAN2 (ECU):     Pin7 H  Pin8 L  Pin9 shield
 7 CAN2H  8 CAN2L 9 CAN2GND      UART (SERVICE): Pin10 TX Pin11 RX Pin12 GND
10 TX    11 RX   12 GND          FUSE 7.5A ATO · NEVER bond the four grounds together
```
