# TP-CAN-2I — KiCad Project

This folder is the editable PCB source. Because the board cannot be auto-routed blind, it is
delivered as a **validated 4-layer mechanical starting board** plus the complete electrical
definition needed to finish it in KiCad 7/8.

## Files
| File | What it is |
|---|---|
| `TP-CAN-2I.kicad_pcb` | 4-layer board: enclosure-matched outline, M3 mounting holes, **isolation slots**, stackup, branding, and **labelled placement guides** (Dwgs.User). Generated + self-validated (round-trips through the parser). |
| `TP-CAN-2I.kicad_pro` | Project: design rules, **net classes** (Power / CAN_LOGIC / CAN1_ISO / CAN2_ISO) and net-class auto-assign patterns. |
| `TP-CAN-2I.kicad_dru` | Custom DRC rules enforcing **≥4 mm isolation creepage**. |
| `generate_board.py` | Regenerates the mechanical board (needs `pip install kiutils`). Edit dimensions/placement here. |

## Finish workflow (in KiCad)
1. **Open** `TP-CAN-2I.kicad_pro`. The PCB opens with the outline, holes, slots, and a
   placement guide rectangle + label for every major block (matches `../BOM.csv` ref-des).
2. **Schematic capture:** create `TP-CAN-2I.kicad_sch` and enter the design from
   [`../Schematic_Design.md`](../Schematic_Design.md) (net-by-net, by pin name). Assign each
   symbol the **Footprint** listed in [`../BOM.csv`](../BOM.csv).
3. **Update PCB from schematic** (F8). Components land; drag each onto its labelled guide.
4. **Route** per [`../Fabrication_Notes.md`](../Fabrication_Notes.md):
   ground plane on In1.Cu, power on In2.Cu, CAN pairs coupled, tight buck hot-loop, and
   **nothing crossing the isolation barrier** (the DRC rules will flag violations).
5. **DRC** → fix → **export** Gerbers/drill/CPL to `../Production` (see fab checklist).

## Why it's built this way
KiCad is not installed in the authoring environment, so a hand-routed board could not be
verified. Everything that *can* be made exact and machine-checked — outline, stackup, layers,
mounting, isolation slots, net classes, DRC rules, BOM, and the full net list — is provided
and validated. The remaining step (symbol placement + copper routing) is interactive work that
belongs in KiCad with a human confirming the isolation layout on-screen.
