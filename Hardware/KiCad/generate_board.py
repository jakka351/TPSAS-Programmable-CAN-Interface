#!/usr/bin/env python3
"""
Tester Present TP-CAN-2I -- KiCad mechanical board generator.

Produces TP-CAN-2I.kicad_pcb: a 4-layer board with the enclosure-matched outline,
PCB mounting holes, galvanic-isolation milling slots, branding, and labelled
component placement guides (courtyards on Dwgs.User + ref text).

This is the MECHANICAL starting board. Finish in KiCad: capture the schematic from
Schematic_Design.md, assign footprints from BOM.csv, "Update PCB from schematic",
place to the guides below, then route per Fabrication_Notes.md and the .kicad_dru rules.

Run:  python generate_board.py
"""
import math, uuid
from kiutils.board import Board
from kiutils.items.brditems import LayerToken
from kiutils.items.gritems import GrLine, GrArc, GrCircle, GrRect, GrText
from kiutils.items.common import Position, TitleBlock, Effects

# ---------------------------------------------------------------- board params
W, H = 70.0, 50.0          # board envelope (mm)
R = 3.0                     # corner radius
K = R * (1 - math.sqrt(0.5))  # rounded-corner mid offset (0.2929*R)
OUT = "Edge.Cuts"
SILK = "F.SilkS"
GUIDE = "Dwgs.User"
FAB = "F.Fab"

def uid():
    return str(uuid.uuid4())

def P(x, y, a=None):
    return Position(X=round(x, 3), Y=round(y, 3), angle=a)

board = Board().create_new()
board.general.thickness = 1.6
board.titleBlock = TitleBlock(
    title="TP-CAN-2I  Programmable Dual-CAN Inline Interface",
    date="2026-05-31", revision="A",
    company="Tester Present Specialist Automotive Solutions",
    comments={1: "(c) 2026 Jack Leighton - Designed in Australia",
              2: "MECHANICAL STARTING BOARD - finish capture/place/route in KiCad"})

# ---- make it a 4-layer stack: insert In1.Cu (GND) and In2.Cu (PWR) -----------
new_layers = []
for lt in board.layers:
    new_layers.append(lt)
    if lt.name == "F.Cu":
        new_layers.append(LayerToken(ordinal=1, name="In1.Cu", type="signal", userName="GND"))
        new_layers.append(LayerToken(ordinal=2, name="In2.Cu", type="signal", userName="PWR"))
board.layers = new_layers

gi = board.graphicItems

# ---------------------------------------------------------------- outline -----
edges = [
    GrLine(start=P(R, 0),     end=P(W - R, 0),     layer=OUT, width=0.12, tstamp=uid()),  # top
    GrLine(start=P(W, R),     end=P(W, H - R),     layer=OUT, width=0.12, tstamp=uid()),  # right
    GrLine(start=P(W - R, H), end=P(R, H),         layer=OUT, width=0.12, tstamp=uid()),  # bottom
    GrLine(start=P(0, H - R), end=P(0, R),         layer=OUT, width=0.12, tstamp=uid()),  # left
]
arcs = [
    GrArc(start=P(R, 0),     mid=P(K, K),             end=P(0, R),         layer=OUT, width=0.12, tstamp=uid()),
    GrArc(start=P(W - R, 0), mid=P(W - K, K),         end=P(W, R),         layer=OUT, width=0.12, tstamp=uid()),
    GrArc(start=P(W, H - R), mid=P(W - K, H - K),     end=P(W - R, H),     layer=OUT, width=0.12, tstamp=uid()),
    GrArc(start=P(R, H),     mid=P(K, H - K),         end=P(0, H - R),     layer=OUT, width=0.12, tstamp=uid()),
]
gi.extend(edges + arcs)

# ---------------------------------------------------------------- mtg holes ---
# M3 PCB mounting holes (NPTH) as Edge.Cuts circles + silk rings
MH = [(4.0, 4.0), (W - 4.0, 4.0), (4.0, H - 4.0), (W - 4.0, H - 4.0)]
for (x, y) in MH:
    gi.append(GrCircle(center=P(x, y), end=P(x + 1.6, y), layer=OUT, width=0.12, fill="none", tstamp=uid()))
    gi.append(GrCircle(center=P(x, y), end=P(x + 3.0, y), layer=SILK, width=0.15, fill="none", tstamp=uid()))

# ------------------------------------------------------- isolation slots ------
# Routed slots under each ISO1042B to maximise logic<->bus creepage (>=4 mm).
def slot(x0, y0, x1, y1):
    gi.append(GrRect(start=P(x0, y0), end=P(x1, y1), layer=OUT, width=0.12, fill="none", tstamp=uid()))
slot(57.3, 8.5, 58.7, 15.5)    # under U9 (CAN1 isolator)
slot(57.3, 29.5, 58.7, 36.5)   # under U10 (CAN2 isolator)

# silk isolation-barrier indicator + channel split
gi.append(GrLine(start=P(54.0, 2.0), end=P(54.0, 48.0), layer=SILK, width=0.2, tstamp=uid()))
gi.append(GrText(text="<<  GALVANIC  ISOLATION  BARRIER  >>",
                 position=P(54.0, 24.0, 90), layer=SILK, effects=Effects(), tstamp=uid()))
gi.append(GrLine(start=P(54.0, 24.0), end=P(70.0, 24.0), layer=SILK, width=0.2, tstamp=uid()))

# ----------------------------------------------------- placement guides -------
# (refdes, centre_x, centre_y, width, height, label)
PLACE = [
    ("J1",  4.0, 25.0,  9.0, 9.0,  "USB-C"),
    ("U6", 24.0, 20.0, 18.0, 25.5, "ESP32-S3"),
    ("SW1",12.0, 7.0,  4.0, 4.0,  "BOOT"),
    ("SW2",12.0, 13.0, 4.0, 4.0,  "RST"),
    ("U1", 14.0, 40.0, 10.0, 8.0,  "BUCK 5V"),
    ("U2", 30.0, 42.0, 7.0, 6.0,   "3V3"),
    ("U7", 44.0, 13.0, 9.0, 6.0,   "MCP2518FD-1"),
    ("U8", 44.0, 31.0, 9.0, 6.0,   "MCP2518FD-2"),
    ("Y1", 44.0, 22.0, 4.0, 3.0,   "40MHz"),
    ("U3", 51.0, 6.5,  6.0, 6.0,   "ISO-DCDC1"),
    ("U4", 51.0, 43.0, 6.0, 6.0,   "ISO-DCDC2"),
    ("U9", 58.0, 12.0, 7.0, 6.0,   "ISO1042-1"),
    ("U10",58.0, 33.0, 7.0, 6.0,   "ISO1042-2"),
    ("CMC1",64.5,12.0, 5.0, 5.0,   "CMC/TVS1"),
    ("CMC2",64.5,33.0, 5.0, 5.0,   "CMC/TVS2"),
    ("J2", 40.0, 46.0, 22.0, 5.0,  "HARNESS HDR (DT 12)"),
    ("J3", 24.0, 35.0, 10.0, 3.0,  "UART HDR"),
]
for ref, cx, cy, w, h, label in PLACE:
    x0, y0, x1, y1 = cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2
    gi.append(GrRect(start=P(x0, y0), end=P(x1, y1), layer=GUIDE, width=0.12, fill="none", tstamp=uid()))
    gi.append(GrText(text=f"{ref} {label}", position=P(cx, cy), layer=GUIDE, effects=Effects(), tstamp=uid()))

# antenna keep-out reminder (ESP32 PCB antenna overhangs top edge, no copper under)
gi.append(GrText(text="ANTENNA KEEP-OUT (no copper) ->", position=P(24.0, 4.0), layer=SILK,
                 effects=Effects(), tstamp=uid()))

# ----------------------------------------------------- branding / fab ---------
gi.append(GrText(text="TESTER PRESENT  TP-CAN-2I  Rev A", position=P(35.0, 49.3), layer=SILK,
                 effects=Effects(), tstamp=uid()))
gi.append(GrText(text="(c)2026 Jack Leighton - Made in Australia", position=P(35.0, 25.0), layer=FAB,
                 effects=Effects(), tstamp=uid()))

# ---------------------------------------------------------------- write -------
out = "TP-CAN-2I.kicad_pcb"
board.to_file(out)
print("wrote", out)

# self-validate: re-parse with kiutils
chk = Board().from_file(out)
print("re-parsed OK:", len(chk.graphicItems), "graphic items;",
      len([l for l in chk.layers if l.type == 'signal']), "copper layers")
