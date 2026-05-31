# TP-CAN-2I — Enclosure Mechanical Drawing

**Model:** TP-CAN-2I Programmable Dual-CAN Inline Interface · **Rev A** · units **mm**
© 2026 Jack Leighton — Tester Present Specialist Automotive Solutions · Designed in Australia

Two-part rugged sealed enclosure (base tray + lid). Manufacturing master is the pair of
2-manifold STL meshes in [`STL/`](STL); this sheet dimensions them for quoting, fixturing and
incoming inspection. Parametric source: [`build_enclosure.py`](build_enclosure.py) (authoritative)
and [`TP-CAN-2I_enclosure.scad`](TP-CAN-2I_enclosure.scad) (OpenSCAD reference).

Previews: [base](STL/preview_base.png) · [lid](STL/preview_lid.png) · [assembly](STL/preview_assembly.png)

---

## 1. Overall envelope

| Dimension | Value |
|---|---|
| Body footprint (excl. ears) | **80.0 × 60.0** |
| Mounting footprint (incl. ears) | **112.4 × 72.8** |
| Base height (floor underside → rim) | **24.6** |
| Lid thickness (plate) | **3.0** |
| Assembled height (ear feet → lid top) | **28.4** |
| Wall thickness | **3.0** |
| Floor / lid thickness | **3.0 / 3.0** |
| Outer vertical corner radius | **4.0** |
| Internal usable cavity (W×D×H) | 74 × 54 × 21.6 |
| Est. printed mass (Tough resin, both parts) | ~95 g |

Coordinate origin = lower-left outer corner of the body, +Z up. The PCB cavity lower-left inner
corner is at (3, 3); the **PCB origin** is (5, 5) and the board is 70 × 50.

---

## 2. Base — top view (looking down −Z)

```
            Y
            ▲   ◄──────────────── 80.0 ────────────────►
            │   ╭───────────────────────────────────────╮
   (-9.2,69.2)●  │  ╭(5.5,54.5)              (74.5,54.5)╮ │  ●(89.2,69.2)
        M5 ⌀5.5  │  ○ tower            LED row →         ○ │   M5 ⌀5.5
            │ ╱  │     ┌───────────────────────────────┐   │ ╲
         60.0    │     │   ◦(9,51)            (71,51)◦  │   │
            │    │     │      PCB standoffs  ⌀6.5       │   │  ┌── connector
            │    │     │      M3 insert ⌀4.0 × 4        │   │  │   aperture
            │    │     │                               │   │  ▼   (on +X wall)
            │    │     │   ◦(9,9)             (71,9)◦   │   │ ████  25.0(Y) × 15.0(Z)
   (-9.2,-9.2)●  │  ○(5.5,5.5)              (74.5,5.5)○ │ │  ●(89.2,-9.2)
        M5 ⌀5.5  │  ╰─────────────────────────────────╯ │   M5 ⌀5.5
            │   ╰────────────────█████────────────────────╯
            │        USB-C port ▲ 13.0(X) × 8.0(Z)  (on −Y wall, ctr X=11)
            └───────────────────────────────────────────────────► X
```

**PCB mounting (M3):** 4 × Ø4.0 heat-set insert pilots at **(9,9) (71,9) (9,51) (71,51)** —
pattern **62 × 42**, standoff Ø6.5 × 4.0 tall (PCB seats at Z=7.0).
**Lid-screw towers (M3):** 4 × Ø4.0 insert pilots at **(5.5,5.5) (74.5,5.5) (5.5,54.5) (74.5,54.5)**,
tops flush with rim (Z=24.6).
**Corner ears (M5):** 4 × Ø5.5 thru with Ø10.0 × 3 deep eyelet pocket; centres at
**(−9.2,−9.2) (89.2,−9.2) (−9.2,69.2) (89.2,69.2)** — pattern **98.4 × 78.4**. Ears drop 0.8 below
the floor to act as defined contact feet.

---

## 3. Base — front view (+X end wall, the connector wall)

```
   Z ▲                         80.0 wide (into page = X is depth here; this is the Y–Z face)
     │   ╭──────────────────────────────────────────╮  ◄ rim (Z=24.6)
24.6 │   │  ░░░ gasket groove 1.2w × 1.4d ░░░░░░░░░░ │
     │   │                                          │
21.0 │   │        ┌────────────────────────┐  ▲     │   connector aperture
     │   │   ⊕    │  Deutsch DT-12 window   │  15.0  │   25.0 (Y) × 15.0 (Z)
13.5 │   │ M3⌀3.3 │  25.0 × 15.0            │  ▼     │   flange screws M3 ⌀3.3
     │   │ flange └────────────────────────┘        │   at Y=14.5 & 45.5, Z=13.5
 6.0 │   │  (Y=14.5)        (Y=45.5)                 │   aperture Y 17.5…42.5
     │   │                                          │
 0.0 │   ╰──────────────────────────────────────────╯  ◄ floor underside
     └──────────────────────────────────────────────► Y
         ◄─────────────────── 60.0 ──────────────────►
```

---

## 4. Base — right side view (−Y wall, the USB wall)

```
   Z ▲          ◄──────────────── 80.0 ────────────────►
24.6 │   ╭───────────────────────────────────────────╮  ◄ rim
     │   │░░ gasket groove ░░░░░░░░░░░░░░░░░░░░░░░░░░░ │
12.6 │   │            ┌───────────┐                   │   USB-C service port
 8.6 │   │   PCB ►----│ 13.0×8.0  │  (ctr X=11)       │   13.0 (X) × 8.0 (Z)
 4.6 │   │            └───────────┘                   │   Z 4.6…12.6  (PCB top Z=8.6)
 0.0 │   ╰───────────────────────────────────────────╯
-0.8 │  ▐█▌                                       ▐█▌    ◄ ear feet protrude 0.8
     └───────────────────────────────────────────────► X
```

---

## 5. Lid (printed upside-down: mating face = Z0)

```
   ╭───────────────────────────────────────────╮   80.0 × 60.0 × 3.0 plate, R4 corners
   │ ⊙(5.5,54.5)                    (74.5,54.5)⊙ │   ⊙ = M3 screw: Ø3.4 thru +
   │                                            │       Ø6.2 × 2 counterbore (from top)
   │            ● ● ● ● ●  LED pipes            │
   │            26 31 36 41 46  (Y=48, Ø3.2)    │   5 × Ø3.2 light-pipe holes @ 5 pitch
   │        ┌────────────────────────┐          │
   │        │  recessed label pocket │          │   branding recess 52 × 14 × 0.6 deep
   │        │  "TESTER PRESENT"      │          │       (engrave / printed insert)
   │        └────────────────────────┘          │
   │ ⊙(5.5,5.5)                      (74.5,5.5)⊙ │
   ╰───────────────────────────────────────────╯
   Underside: 4 perimeter registration ribs (tongue) 2.0 wide × 4.0 deep, set 3.2 in from
   the wall, running the mid-span of each edge (clear of corner towers & standoffs).
```

---

## 6. Seal & closure

| Feature | Spec |
|---|---|
| Seal type | Gasket-in-rim + lid registration tongue |
| Gasket groove | width **1.2**, depth **1.4**, centred in the 3.0 rim (0.9–2.1 from outer edge) |
| Recommended gasket | Ø1.5 mm silicone O-cord (continuous loop), or 1.5 mm closed-cell foam tape |
| Lid tongue | 2.0 × 4.0 ribs, 0.2 clearance to inner wall (registration, not the seal face) |
| Closure | 4 × M3 × 10 SHCS, lid → corner-tower heat-set inserts |
| Sealing target | IP65 (dust-tight, low-pressure water jets) with gasket fitted |

---

## 7. Material, process & tolerances

| Item | Spec |
|---|---|
| Primary process | **SLA**, tough/durable resin (Formlabs Tough 2000 or Durable; or Siraya Tech ABS-Like) |
| Production alt. | **SLS PA12 (nylon)** for highest impact/temperature ruggedness |
| General tolerance | ±0.20 (features), ±0.15 (hole Ø) |
| Heat-set inserts | M3 brass, Ø4.0 pilot (e.g., CNC-Kitchen M3×5.7 / McMaster 94180A331) — install with iron |
| Min wall / detail | 1.5 mm honoured throughout; rim lips 0.9 mm (resin-rigid OK) |
| Finish | bead-blast / matte; UV post-cure per resin spec; black or hi-vis safety orange |
| Print orientation | base: floor-down; lid: top-face-down (mating tongue up) — both supported off feet/ribs |

---

## 8. Inspection checklist (first article)

- [ ] Footprint 80.0 × 60.0 ±0.2; assembled height 28.4 ±0.3.
- [ ] M5 ear pattern 98.4 × 78.4; M5 holes Ø5.5 clear, eyelet pockets Ø10.0 × 3.
- [ ] PCB insert pattern 62 × 42; 4 × Ø4.0 inserts seat flush, PCB datum height 7.0.
- [ ] Connector aperture 25.0 × 15.0 at Z 6–21; flange holes Ø3.3 at Y 14.5/45.5.
- [ ] USB-C port 13.0 × 8.0 at Z 4.6–12.6, centred X=11.
- [ ] 5 × LED Ø3.2 at Y=48, X=26/31/36/41/46.
- [ ] Lid closes on towers; gasket groove clean & continuous; tongue clears towers/standoffs.
- [ ] Watertight check (mesh): lid 2-manifold; base closed solid (see `build_enclosure.py` report).
```
