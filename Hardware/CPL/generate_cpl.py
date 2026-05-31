#!/usr/bin/env python3
"""
generate_cpl.py — Component Placement List (CPL / pick-and-place centroid report)
for the TP-CAN-2I PCB.

Outputs:
  TP-CAN-2I_CPL.csv   — flat centroid CSV
  TP-CAN-2I_CPL.pdf   — professional formatted PDF via ReportLab

Run from any directory; outputs land next to this script.
"""

import csv
import os
import sys
from datetime import date
from pathlib import Path

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
SCRIPT_DIR = Path(__file__).resolve().parent
BOM_PATH   = SCRIPT_DIR.parent / "BOM.csv"
CSV_OUT    = SCRIPT_DIR / "TP-CAN-2I_CPL.csv"
PDF_OUT    = SCRIPT_DIR / "TP-CAN-2I_CPL.pdf"

# ---------------------------------------------------------------------------
# PLACE centroids from generate_board.py (refdes → (cx, cy))
# ---------------------------------------------------------------------------
PLACE = [
    ("J1",   4.0,  25.0,  9.0,  9.0,  "USB-C"),
    ("U6",  24.0,  20.0, 18.0, 25.5,  "ESP32-S3"),
    ("SW1", 12.0,   7.0,  4.0,  4.0,  "BOOT"),
    ("SW2", 12.0,  13.0,  4.0,  4.0,  "RST"),
    ("U1",  14.0,  40.0, 10.0,  8.0,  "BUCK 5V"),
    ("U2",  30.0,  42.0,  7.0,  6.0,  "3V3"),
    ("U7",  44.0,  13.0,  9.0,  6.0,  "MCP2518FD-1"),
    ("U8",  44.0,  31.0,  9.0,  6.0,  "MCP2518FD-2"),
    ("Y1",  44.0,  22.0,  4.0,  3.0,  "40MHz"),
    ("U3",  51.0,   6.5,  6.0,  6.0,  "ISO-DCDC1"),
    ("U4",  51.0,  43.0,  6.0,  6.0,  "ISO-DCDC2"),
    ("U9",  58.0,  12.0,  7.0,  6.0,  "ISO1042-1"),
    ("U10", 58.0,  33.0,  7.0,  6.0,  "ISO1042-2"),
    ("CMC1",64.5,  12.0,  5.0,  5.0,  "CMC/TVS1"),
    ("CMC2",64.5,  33.0,  5.0,  5.0,  "CMC/TVS2"),
    ("J2",  40.0,  46.0, 22.0,  5.0,  "HARNESS HDR (DT 12)"),
    ("J3",  24.0,  35.0, 10.0,  3.0,  "UART HDR"),
]

PLACE_MAP = {ref: (cx, cy) for ref, cx, cy, *_ in PLACE}

# ---------------------------------------------------------------------------
# Load BOM
# ---------------------------------------------------------------------------
def load_bom(path: Path):
    rows = []
    with open(path, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)
    return rows

# ---------------------------------------------------------------------------
# Build CPL rows
# ---------------------------------------------------------------------------
CPL_HEADER = ["Designator", "Value", "Package", "MidX_mm", "MidY_mm",
               "Rotation_deg", "Layer"]

def build_cpl(bom_rows):
    cpl = []
    real_count = 0
    tbd_count  = 0

    for row in bom_rows:
        ref      = row["Ref"].strip()
        qty      = row["Qty"].strip()
        value    = row["Value"].strip()
        mpn      = row.get("MPN", "").strip()
        footprint= row.get("Footprint", "").strip()
        desc     = row.get("Description", "").strip()

        # Skip purely mechanical/PCB entries
        if ref == "PCB":
            continue

        # Annotate qty > 1 in value field for passive groups
        try:
            q = int(qty)
        except ValueError:
            q = 1

        display_value = value
        if q > 1:
            display_value = f"{value} (x{q})"

        if ref in PLACE_MAP:
            cx, cy = PLACE_MAP[ref]
            mid_x  = f"{cx:.3f}"
            mid_y  = f"{cy:.3f}"
            real_count += 1
        else:
            mid_x = "TBD"
            mid_y = "TBD"
            tbd_count += 1

        cpl.append([ref, display_value, footprint, mid_x, mid_y, "0", "Top"])

    return cpl, real_count, tbd_count

# ---------------------------------------------------------------------------
# Write CSV
# ---------------------------------------------------------------------------
def write_csv(cpl_rows, path: Path):
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(CPL_HEADER)
        writer.writerows(cpl_rows)
    print(f"  CSV written: {path}  ({len(cpl_rows)} data rows)")

# ---------------------------------------------------------------------------
# Write PDF
# ---------------------------------------------------------------------------
def write_pdf(cpl_rows, path: Path):
    from reportlab.lib.pagesizes import A4, landscape
    from reportlab.lib.units import mm
    from reportlab.lib import colors
    from reportlab.platypus import (SimpleDocTemplate, Table, TableStyle,
                                    Paragraph, Spacer)
    from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
    from reportlab.lib.enums import TA_CENTER, TA_LEFT, TA_RIGHT

    PAGE_W, PAGE_H = landscape(A4)
    MARGIN = 15 * mm

    # ---- styles ----------------------------------------------------------------
    styles = getSampleStyleSheet()

    title_style = ParagraphStyle(
        "CPLTitle",
        parent=styles["Normal"],
        fontSize=14,
        fontName="Helvetica-Bold",
        alignment=TA_CENTER,
        spaceAfter=2 * mm,
    )
    subtitle_style = ParagraphStyle(
        "CPLSubtitle",
        parent=styles["Normal"],
        fontSize=10,
        fontName="Helvetica-Bold",
        alignment=TA_CENTER,
        spaceAfter=1 * mm,
    )
    meta_style = ParagraphStyle(
        "CPLMeta",
        parent=styles["Normal"],
        fontSize=9,
        fontName="Helvetica",
        alignment=TA_CENTER,
        spaceAfter=4 * mm,
    )
    footer_note_style = ParagraphStyle(
        "FooterNote",
        parent=styles["Normal"],
        fontSize=7,
        fontName="Helvetica-Oblique",
        alignment=TA_CENTER,
    )

    # ---- page number / footer callback ----------------------------------------
    class _PageFooter:
        def __init__(self, note_text):
            self.note = note_text

        def __call__(self, canvas, doc):
            canvas.saveState()
            w, h = canvas._pagesize
            # Horizontal rule
            canvas.setStrokeColor(colors.HexColor("#444444"))
            canvas.setLineWidth(0.5)
            canvas.line(MARGIN, 14 * mm, w - MARGIN, 14 * mm)
            # Note text (left)
            canvas.setFont("Helvetica-Oblique", 7)
            canvas.setFillColor(colors.HexColor("#555555"))
            canvas.drawString(MARGIN, 9 * mm,
                              f"NOTE: {self.note}")
            # Page number (right)
            canvas.setFont("Helvetica", 7)
            canvas.setFillColor(colors.HexColor("#333333"))
            page_str = f"Page {doc.page}"
            canvas.drawRightString(w - MARGIN, 9 * mm, page_str)
            canvas.restoreState()

    footer_note = (
        "Centroids are nominal from the mechanical placement guide; "
        "regenerate from the routed KiCad board for production pick-and-place."
    )
    on_page = _PageFooter(footer_note)

    # ---- document --------------------------------------------------------------
    doc = SimpleDocTemplate(
        str(path),
        pagesize=landscape(A4),
        leftMargin=MARGIN,
        rightMargin=MARGIN,
        topMargin=MARGIN,
        bottomMargin=22 * mm,       # room for footer
        title="TP-CAN-2I Component Placement List",
        author="Jack Leighton — Tester Present Specialist Automotive Solutions",
        subject="CPL Rev A",
        creator="generate_cpl.py / ReportLab",
    )

    # ---- build table data ------------------------------------------------------
    col_headers = CPL_HEADER  # ["Designator","Value","Package","MidX_mm","MidY_mm","Rotation_deg","Layer"]
    table_data = [col_headers] + cpl_rows

    # Column widths (mm) → points — total ≤ usable width
    usable_w = PAGE_W - 2 * MARGIN
    # Designator, Value, Package, MidX, MidY, Rot, Layer
    col_w_mm = [22, 55, 70, 22, 22, 22, 16]
    col_w    = [w * mm for w in col_w_mm]
    # Scale to fit exactly
    total = sum(col_w)
    col_w = [w * (usable_w / total) for w in col_w]

    HEADER_BG  = colors.HexColor("#1A3A5C")
    HEADER_FG  = colors.white
    STRIPE_ODD = colors.HexColor("#EBF1F8")
    STRIPE_EVN = colors.white
    GRID_CLR   = colors.HexColor("#AAAAAA")
    TBD_CLR    = colors.HexColor("#CC4400")

    tbl_style_cmds = [
        # Header row
        ("BACKGROUND", (0, 0), (-1, 0), HEADER_BG),
        ("TEXTCOLOR",  (0, 0), (-1, 0), HEADER_FG),
        ("FONTNAME",   (0, 0), (-1, 0), "Helvetica-Bold"),
        ("FONTSIZE",   (0, 0), (-1, 0), 8),
        ("ALIGN",      (0, 0), (-1, 0), "CENTER"),
        ("VALIGN",     (0, 0), (-1, -1), "MIDDLE"),
        # Data rows font
        ("FONTNAME",   (0, 1), (-1, -1), "Helvetica"),
        ("FONTSIZE",   (0, 1), (-1, -1), 7.5),
        # Alignment
        ("ALIGN",      (0, 1), (0, -1), "LEFT"),    # Designator
        ("ALIGN",      (1, 1), (1, -1), "LEFT"),    # Value
        ("ALIGN",      (2, 1), (2, -1), "LEFT"),    # Package
        ("ALIGN",      (3, 1), (4, -1), "CENTER"),  # MidX/MidY
        ("ALIGN",      (5, 1), (6, -1), "CENTER"),  # Rot/Layer
        # Grid
        ("GRID",       (0, 0), (-1, -1), 0.5, GRID_CLR),
        ("ROWBACKGROUNDS", (0, 1), (-1, -1), [STRIPE_ODD, STRIPE_EVN]),
        # Padding
        ("TOPPADDING",    (0, 0), (-1, -1), 3),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
        ("LEFTPADDING",   (0, 0), (-1, -1), 4),
        ("RIGHTPADDING",  (0, 0), (-1, -1), 4),
        # Repeat header on every page
        ("REPEATROWS", (0, 0), (0, 0)),
    ]

    # Highlight TBD cells
    for i, row in enumerate(cpl_rows, start=1):
        if row[3] == "TBD":
            tbl_style_cmds.append(("TEXTCOLOR", (3, i), (4, i), TBD_CLR))
            tbl_style_cmds.append(("FONTNAME",  (3, i), (4, i), "Helvetica-Bold"))

    table = Table(table_data, colWidths=col_w, repeatRows=1)
    table.setStyle(TableStyle(tbl_style_cmds))

    # ---- title block -----------------------------------------------------------
    story = [
        Paragraph("TESTER PRESENT — TP-CAN-2I Programmable Dual-CAN Inline Interface",
                  title_style),
        Paragraph("Component Placement List (CPL) — Rev A", subtitle_style),
        Paragraph(
            f"Date: 2026-06-01 &nbsp;&nbsp;|&nbsp;&nbsp; "
            "© 2026 Jack Leighton — Designed in Australia",
            meta_style,
        ),
        table,
    ]

    doc.build(story, onFirstPage=on_page, onLaterPages=on_page)
    print(f"  PDF written: {path}")

# ---------------------------------------------------------------------------
# Validate PDF
# ---------------------------------------------------------------------------
def validate_pdf(path: Path):
    if not path.exists():
        print("  FAIL: PDF not found on disk.", file=sys.stderr)
        return False

    size = path.stat().st_size
    with open(path, "rb") as f:
        header = f.read(5)

    if header != b"%PDF-":
        print(f"  FAIL: unexpected PDF header bytes: {header!r}", file=sys.stderr)
        return False

    if size < 2048:
        print(f"  FAIL: PDF suspiciously small ({size} bytes).", file=sys.stderr)
        return False

    # Count pages via a simple scan for /Type /Page occurrences
    with open(path, "rb") as f:
        content = f.read()
    page_count = content.count(b"/Type /Page")
    if page_count == 0:
        # Some writers use /Type/Page without spaces
        page_count = content.count(b"/Type/Page")
    if page_count == 0:
        print("  WARN: could not detect /Type /Page; assuming ≥1 page from file size.",
              file=sys.stderr)
        page_count = 1  # file size check already passed

    print(f"  VALIDATE OK: {size:,} bytes, header=%PDF-, ~{page_count} page(s)")
    return True

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    print("=== TP-CAN-2I CPL Generator ===")
    print(f"  BOM  : {BOM_PATH}")
    print(f"  Output dir: {SCRIPT_DIR}")

    bom_rows = load_bom(BOM_PATH)
    print(f"  BOM loaded: {len(bom_rows)} entries")

    cpl_rows, real_count, tbd_count = build_cpl(bom_rows)
    total = real_count + tbd_count
    print(f"  CPL rows: {total} total  |  {real_count} with real centroids  |  {tbd_count} TBD")

    write_csv(cpl_rows, CSV_OUT)
    write_pdf(cpl_rows, PDF_OUT)

    ok = validate_pdf(PDF_OUT)
    if not ok:
        sys.exit(1)

    print()
    print("=== SUMMARY ===")
    print(f"  Total CPL rows      : {total}")
    print(f"  Real centroids      : {real_count}")
    print(f"  TBD (passives etc.) : {tbd_count}")
    print(f"  CSV : {CSV_OUT}")
    print(f"  PDF : {PDF_OUT}")

if __name__ == "__main__":
    main()
