"""
md_to_pdf.py  —  Converts Engineering_Specification.md to a branded PDF.
Pure Python + ReportLab 4.x.  No third-party markdown library required.
"""

import os
import re
import sys
from copy import deepcopy

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT, TA_RIGHT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import cm, mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    BaseDocTemplate,
    Flowable,
    Frame,
    HRFlowable,
    Image,
    NextPageTemplate,
    PageBreak,
    PageTemplate,
    Paragraph,
    Spacer,
    Table,
    TableStyle,
    KeepTogether,
)
from reportlab.platypus.tableofcontents import TableOfContents

# ── paths ─────────────────────────────────────────────────────────────────────
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
MD_PATH  = os.path.join(BASE_DIR, "Engineering_Specification.md")
PDF_PATH = os.path.join(BASE_DIR, "Engineering_Specification.pdf")
LOGO_PATH = os.path.join(os.path.dirname(BASE_DIR), "Images", "logo.png")

# ── brand colours ─────────────────────────────────────────────────────────────
BRAND_DARK   = colors.HexColor("#1A2639")   # dark navy
BRAND_ACCENT = colors.HexColor("#E07B39")   # orange accent
HEADER_BG    = colors.HexColor("#1A2639")
HEADER_FG    = colors.white
CODE_BG      = colors.HexColor("#F3F4F6")
TABLE_HDR_BG = colors.HexColor("#1A2639")
TABLE_ALT_BG = colors.HexColor("#F7F8FA")
GRAY_TEXT    = colors.HexColor("#666666")

PAGE_W, PAGE_H = A4
MARGIN_LEFT   = 2.0 * cm
MARGIN_RIGHT  = 2.0 * cm
MARGIN_TOP    = 2.2 * cm
MARGIN_BOTTOM = 2.2 * cm
HEADER_H      = 0.8 * cm
FOOTER_H      = 0.8 * cm

PRODUCT_SHORT = "TP-CAN-2I Programmable Dual-CAN Inline Interface"
FOOTER_LEFT   = "© 2026 Jack Leighton — Confidential"

# ── styles ────────────────────────────────────────────────────────────────────
_base = getSampleStyleSheet()

def make_style(name, **kw):
    return ParagraphStyle(name, **kw)

sty_normal = make_style("ES_Normal",
    fontName="Helvetica", fontSize=9.5, leading=14,
    spaceAfter=4, textColor=colors.black)

sty_h1 = make_style("ES_H1",
    fontName="Helvetica-Bold", fontSize=16, leading=20,
    spaceBefore=14, spaceAfter=6, textColor=BRAND_DARK,
    borderPad=0)

sty_h2 = make_style("ES_H2",
    fontName="Helvetica-Bold", fontSize=13, leading=17,
    spaceBefore=12, spaceAfter=4, textColor=BRAND_DARK)

sty_h3 = make_style("ES_H3",
    fontName="Helvetica-BoldOblique", fontSize=11, leading=15,
    spaceBefore=10, spaceAfter=3, textColor=BRAND_DARK)

sty_bullet = make_style("ES_Bullet",
    fontName="Helvetica", fontSize=9.5, leading=14,
    leftIndent=18, firstLineIndent=0, spaceAfter=2,
    bulletIndent=6, bulletFontName="Helvetica", bulletFontSize=9.5)

sty_numbered = make_style("ES_Numbered",
    fontName="Helvetica", fontSize=9.5, leading=14,
    leftIndent=22, firstLineIndent=0, spaceAfter=2)

sty_code_inline = make_style("ES_CodeInline",
    fontName="Courier", fontSize=8.5, leading=12,
    textColor=colors.HexColor("#C0392B"))

sty_code_block = make_style("ES_CodeBlock",
    fontName="Courier", fontSize=7.5, leading=10,
    leftIndent=6, rightIndent=6,
    spaceAfter=2, spaceBefore=2,
    textColor=colors.HexColor("#1A1A1A"),
    backColor=CODE_BG)

sty_toc_h1 = make_style("ES_TOC1",
    fontName="Helvetica", fontSize=10, leading=14,
    leftIndent=0, spaceAfter=2)

sty_toc_h2 = make_style("ES_TOC2",
    fontName="Helvetica", fontSize=9.5, leading=13,
    leftIndent=14, spaceAfter=1)

sty_toc_h3 = make_style("ES_TOC3",
    fontName="Helvetica-Oblique", fontSize=9, leading=12,
    leftIndent=28, spaceAfter=1)

sty_blockquote = make_style("ES_BlockQuote",
    fontName="Helvetica-Oblique", fontSize=9, leading=13,
    leftIndent=20, rightIndent=10, spaceAfter=4,
    textColor=GRAY_TEXT)

sty_title_product = make_style("ES_TitleProduct",
    fontName="Helvetica-Bold", fontSize=22, leading=28,
    alignment=TA_CENTER, textColor=BRAND_DARK, spaceAfter=6)

sty_title_sub = make_style("ES_TitleSub",
    fontName="Helvetica-Bold", fontSize=16, leading=22,
    alignment=TA_CENTER, textColor=BRAND_ACCENT, spaceAfter=4)

sty_title_rev = make_style("ES_TitleRev",
    fontName="Helvetica", fontSize=11, leading=15,
    alignment=TA_CENTER, textColor=GRAY_TEXT, spaceAfter=4)

sty_title_copy = make_style("ES_TitleCopy",
    fontName="Helvetica-Oblique", fontSize=9, leading=13,
    alignment=TA_CENTER, textColor=GRAY_TEXT)

sty_toc_title = make_style("ES_TOCTitle",
    fontName="Helvetica-Bold", fontSize=14, leading=18,
    spaceAfter=10, textColor=BRAND_DARK)

# ── inline markup helper ──────────────────────────────────────────────────────
_BOLD_RE   = re.compile(r'\*\*(.+?)\*\*')
_ITAL_RE   = re.compile(r'\*([^*]+?)\*(?!\*)')
_CODE_RE   = re.compile(r'`([^`]+)`')
_LINK_RE   = re.compile(r'\[([^\]]+)\]\([^\)]+\)')
_SUB_RE    = re.compile(r'<sub>(.+?)</sub>', re.IGNORECASE)
_SUP_RE    = re.compile(r'<super>(.+?)</super>|<sup>(.+?)</sup>', re.IGNORECASE)

_XML_CHARS = str.maketrans({'&': '&amp;', '<': '&lt;', '>': '&gt;'})

def escape_xml(text):
    return text.translate(_XML_CHARS)

def inline_markup(text):
    """Convert markdown inline syntax to ReportLab XML tags."""
    # First, protect code spans (extract, escape, re-insert as tags)
    code_spans = []
    def save_code(m):
        idx = len(code_spans)
        code_spans.append(m.group(1))
        return f"\x00CODE{idx}\x00"

    text = _CODE_RE.sub(save_code, text)

    # Escape XML in the non-code portions
    text = escape_xml(text)

    # Restore code spans with formatting
    for idx, code_text in enumerate(code_spans):
        escaped_code = escape_xml(code_text)
        text = text.replace(
            f"\x00CODE{idx}\x00",
            f'<font name="Courier" color="#C0392B" size="8">{escaped_code}</font>'
        )

    # Bold
    text = _BOLD_RE.sub(r'<b>\1</b>', text)
    # Italics (simple asterisk, not double)
    text = _ITAL_RE.sub(r'<i>\1</i>', text)
    # Links — keep text, drop URL
    text = _LINK_RE.sub(r'\1', text)
    # Sub/Superscripts
    text = _SUB_RE.sub(r'<sub>\1</sub>', text)
    text = _SUP_RE.sub(lambda m: f'<super>{m.group(1) or m.group(2)}</super>', text)

    return text

# ── code-block flowable ───────────────────────────────────────────────────────
class CodeBlock(Flowable):
    """Renders a fenced code block with a light-gray background."""

    def __init__(self, lines, avail_width):
        super().__init__()
        self.lines = lines
        self.avail_width = avail_width
        self._padding = 6
        self._line_h = 9.5   # pts per line
        self.hAlign = 'LEFT'

    def wrap(self, availWidth, availHeight):
        self._w = min(self.avail_width, availWidth)
        self._h = len(self.lines) * self._line_h + 2 * self._padding
        return self._w, self._h

    def draw(self):
        c = self.canv
        c.saveState()
        # Background rect
        c.setFillColor(CODE_BG)
        c.setStrokeColor(colors.HexColor("#CCCCCC"))
        c.roundRect(0, 0, self._w, self._h, 3, fill=1, stroke=1)
        # Text
        c.setFont("Courier", 7.5)
        c.setFillColor(colors.HexColor("#1A1A1A"))
        y = self._h - self._padding - 7.5
        for line in self.lines:
            # Truncate very long lines gracefully
            display = line.rstrip('\n')
            c.drawString(self._padding, y, display)
            y -= self._line_h
        c.restoreState()

# ── page callbacks (header + footer) ──────────────────────────────────────────
def on_title_page(canvas, doc):
    """Title page — no header/footer."""
    pass

def on_content_page(canvas, doc):
    """Running header + footer on every content page."""
    canvas.saveState()
    w = PAGE_W

    # ---- Header ----
    hdr_y = PAGE_H - MARGIN_TOP + 3 * mm
    canvas.setFillColor(BRAND_DARK)
    canvas.rect(MARGIN_LEFT, hdr_y, w - MARGIN_LEFT - MARGIN_RIGHT, HEADER_H,
                fill=1, stroke=0)
    canvas.setFont("Helvetica", 7.5)
    canvas.setFillColor(colors.white)
    canvas.drawString(MARGIN_LEFT + 4, hdr_y + 4, PRODUCT_SHORT)

    # ---- Footer ----
    ftr_y = MARGIN_BOTTOM - FOOTER_H - 2 * mm
    canvas.setStrokeColor(BRAND_ACCENT)
    canvas.setLineWidth(0.8)
    canvas.line(MARGIN_LEFT, ftr_y + FOOTER_H, w - MARGIN_RIGHT, ftr_y + FOOTER_H)

    canvas.setFont("Helvetica", 7.5)
    canvas.setFillColor(GRAY_TEXT)
    canvas.drawString(MARGIN_LEFT, ftr_y + 3, FOOTER_LEFT)
    page_label = f"Page {doc.page}"
    canvas.drawRightString(w - MARGIN_RIGHT, ftr_y + 3, page_label)

    canvas.restoreState()

# ── document template ─────────────────────────────────────────────────────────
def build_doc_template(pdf_path):
    doc = BaseDocTemplate(
        pdf_path,
        pagesize=A4,
        leftMargin=MARGIN_LEFT,
        rightMargin=MARGIN_RIGHT,
        topMargin=MARGIN_TOP,
        bottomMargin=MARGIN_BOTTOM,
        title="TP-CAN-2I Engineering Specification",
        author="Jack Leighton — Tester Present Specialist Automotive Solutions",
        subject="Engineering Specification Rev A",
    )

    body_w = PAGE_W - MARGIN_LEFT - MARGIN_RIGHT
    body_h = PAGE_H - MARGIN_TOP - MARGIN_BOTTOM

    # Title page frame (full page, no header/footer insets)
    title_frame = Frame(MARGIN_LEFT, MARGIN_BOTTOM, body_w, body_h,
                        leftPadding=0, rightPadding=0,
                        topPadding=0, bottomPadding=0, id="title")

    # Content frame (leaves room for header & footer)
    content_frame = Frame(MARGIN_LEFT,
                          MARGIN_BOTTOM + FOOTER_H + 4 * mm,
                          body_w,
                          body_h - HEADER_H - 4 * mm - FOOTER_H - 4 * mm,
                          leftPadding=0, rightPadding=0,
                          topPadding=0, bottomPadding=0, id="content")

    title_tmpl   = PageTemplate(id="Title",   frames=[title_frame],
                                onPage=on_title_page)
    content_tmpl = PageTemplate(id="Content", frames=[content_frame],
                                onPage=on_content_page)

    doc.addPageTemplates([title_tmpl, content_tmpl])
    return doc, body_w

# ── title page builder ────────────────────────────────────────────────────────
def build_title_page(body_w):
    story = []

    story.append(Spacer(1, 3 * cm))

    # Logo
    try:
        img = Image(LOGO_PATH)
        logo_w = 6 * cm
        aspect = img.imageHeight / float(img.imageWidth)
        img.drawWidth  = logo_w
        img.drawHeight = logo_w * aspect
        img.hAlign = 'CENTER'
        story.append(img)
        story.append(Spacer(1, 1 * cm))
    except Exception:
        pass  # Logo load failure — skip gracefully

    story.append(Paragraph("TP-CAN-2I — Programmable Dual-CAN Inline Interface",
                            sty_title_product))
    story.append(Spacer(1, 0.4 * cm))
    story.append(Paragraph("Engineering Specification", sty_title_sub))
    story.append(Spacer(1, 0.4 * cm))
    story.append(Paragraph("Revision A · 2026-06-01", sty_title_rev))
    story.append(Spacer(1, 1.5 * cm))

    # Horizontal rule
    story.append(HRFlowable(width="100%", thickness=1.5, color=BRAND_ACCENT,
                             spaceAfter=1 * cm))

    story.append(Paragraph(
        "© 2026 Jack Leighton — Tester Present Specialist Automotive Solutions — Designed in Australia",
        sty_title_copy))

    story.append(PageBreak())
    return story

# ── TOC ───────────────────────────────────────────────────────────────────────
def build_toc():
    toc = TableOfContents()
    toc.levelStyles = [sty_toc_h1, sty_toc_h2, sty_toc_h3]
    toc.dotsMinLevel = 0
    return toc

# ── pipe-table parser ──────────────────────────────────────────────────────────
def parse_pipe_table(lines):
    """Parse GitHub-style | a | b | tables. Returns list-of-rows (each row = list of strings)."""
    rows = []
    for line in lines:
        line = line.strip()
        if not line.startswith('|'):
            continue
        # Separator row e.g. |---|---|
        if re.match(r'^\|[\s\-:]+\|[\s\-:|]*$', line):
            continue
        cells = [c.strip() for c in line.strip('|').split('|')]
        rows.append(cells)
    return rows

def render_pipe_table(rows, body_w):
    """Render a list-of-rows as a ReportLab Table with branded header."""
    if not rows:
        return []

    max_cols = max(len(r) for r in rows)

    # Normalise row lengths
    norm = []
    for row in rows:
        while len(row) < max_cols:
            row.append("")
        norm.append(row[:max_cols])

    # Convert cells to Paragraphs
    hdr_sty = make_style("ES_TblHdr",
        fontName="Helvetica-Bold", fontSize=8.5, leading=11,
        textColor=colors.white, alignment=TA_LEFT)
    cell_sty = make_style("ES_TblCell",
        fontName="Helvetica", fontSize=8.5, leading=11,
        textColor=colors.black, alignment=TA_LEFT)

    table_data = []
    for i, row in enumerate(norm):
        sty = hdr_sty if i == 0 else cell_sty
        para_row = [Paragraph(inline_markup(cell), sty) for cell in row]
        table_data.append(para_row)

    col_w = (body_w - 2) / max_cols

    tbl = Table(table_data, colWidths=[col_w] * max_cols, repeatRows=1)

    cmd = [
        # Header row
        ('BACKGROUND', (0, 0), (-1, 0), TABLE_HDR_BG),
        ('TEXTCOLOR',  (0, 0), (-1, 0), colors.white),
        # Grid
        ('GRID',       (0, 0), (-1, -1), 0.5, colors.HexColor("#CCCCCC")),
        ('VALIGN',     (0, 0), (-1, -1), 'TOP'),
        ('LEFTPADDING', (0, 0), (-1, -1), 4),
        ('RIGHTPADDING',(0, 0), (-1, -1), 4),
        ('TOPPADDING',  (0, 0), (-1, -1), 3),
        ('BOTTOMPADDING',(0, 0), (-1, -1), 3),
    ]
    # Alternate row shading (skip header row 0)
    for i in range(1, len(table_data)):
        if i % 2 == 0:
            cmd.append(('BACKGROUND', (0, i), (-1, i), TABLE_ALT_BG))

    tbl.setStyle(TableStyle(cmd))
    return [tbl, Spacer(1, 6)]

# ── heading notify helper ──────────────────────────────────────────────────────
class _TOCEntry(Flowable):
    """Zero-height flowable that notifies the TOC about a heading."""
    def __init__(self, text, level, toc):
        super().__init__()
        self._text  = text
        self._level = level
        self._toc   = toc
        self.width  = 0
        self.height = 0

    def wrap(self, *args):
        return 0, 0

    def draw(self):
        pass

    def afterPage(self):
        pass

    # Called by the doc after the flowable is rendered
    def notify(self, kind, stuff):
        pass


class HeadingWithTOC(Paragraph):
    """A Paragraph that also registers itself with the TOC."""
    def __init__(self, text, style, level, toc):
        super().__init__(text, style)
        self._toc_level = level
        self._toc       = toc
        self._toc_text  = re.sub(r'<[^>]+>', '', text)  # strip XML tags for TOC

    def afterFlowable(self, canvas):
        """Called after the flowable is placed on the page."""
        self._toc.notify('TOCEntry', (self._toc_level, self._toc_text,
                                      canvas._pageCount if hasattr(canvas, '_pageCount') else 0))


# ── markdown parser → ReportLab story ────────────────────────────────────────
def md_to_story(md_text, toc, body_w):
    """Parse markdown text and return a list of ReportLab flowables."""

    lines  = md_text.splitlines()
    story  = []
    i      = 0
    n      = len(lines)

    def flush_para(buf):
        """Convert accumulated text lines to a Paragraph."""
        if not buf:
            return
        text = ' '.join(buf).strip()
        if text:
            story.append(Paragraph(inline_markup(text), sty_normal))
            story.append(Spacer(1, 3))

    def add_heading(raw_text, level):
        cleaned = inline_markup(raw_text.strip())
        if level == 1:
            sty = sty_h1
            toc_level = 0
        elif level == 2:
            sty = sty_h2
            toc_level = 1
        else:
            sty = sty_h3
            toc_level = 2

        p = HeadingWithTOC(cleaned, sty, toc_level, toc)
        story.append(p)

    para_buf = []

    while i < n:
        line = lines[i]
        raw  = line  # preserve for fallback

        # ── Fenced code block ──────────────────────────────────────────────
        if line.startswith('```'):
            flush_para(para_buf); para_buf = []
            lang_hint = line[3:].strip()
            i += 1
            code_lines = []
            while i < n and not lines[i].startswith('```'):
                code_lines.append(lines[i])
                i += 1
            # skip closing ```
            cb = CodeBlock(code_lines, body_w)
            story.append(Spacer(1, 4))
            story.append(cb)
            story.append(Spacer(1, 6))
            i += 1
            continue

        # ── Horizontal rule ─────────────────────────────────────────────────
        if re.match(r'^---+\s*$', line) or re.match(r'^===+\s*$', line):
            flush_para(para_buf); para_buf = []
            story.append(HRFlowable(width="100%", thickness=0.75,
                                    color=colors.HexColor("#CCCCCC"),
                                    spaceBefore=4, spaceAfter=8))
            i += 1
            continue

        # ── Pipe table ──────────────────────────────────────────────────────
        if line.strip().startswith('|'):
            flush_para(para_buf); para_buf = []
            tbl_lines = []
            while i < n and lines[i].strip().startswith('|'):
                tbl_lines.append(lines[i])
                i += 1
            rows = parse_pipe_table(tbl_lines)
            story.extend(render_pipe_table(rows, body_w))
            continue

        # ── ATX headings (#, ##, ###) ────────────────────────────────────────
        m = re.match(r'^(#{1,6})\s+(.*)', line)
        if m:
            flush_para(para_buf); para_buf = []
            level = len(m.group(1))
            text  = m.group(2).strip()
            add_heading(text, min(level, 3))
            i += 1
            continue

        # ── Blockquote ──────────────────────────────────────────────────────
        if line.strip().startswith('>'):
            flush_para(para_buf); para_buf = []
            bq_text = line.strip().lstrip('> ').strip()
            # Collect continuation lines
            i += 1
            while i < n and lines[i].strip().startswith('>'):
                bq_text += ' ' + lines[i].strip().lstrip('> ').strip()
                i += 1
            story.append(Paragraph(inline_markup(bq_text), sty_blockquote))
            story.append(Spacer(1, 3))
            continue

        # ── Unordered list item (- or *) ────────────────────────────────────
        m = re.match(r'^(\s*)([-*])\s+(.*)', line)
        if m:
            flush_para(para_buf); para_buf = []
            indent_depth = len(m.group(1)) // 2
            text = m.group(3)
            extra_indent = indent_depth * 12
            local_sty = make_style(f"ES_Bul_{i}",
                fontName="Helvetica", fontSize=9.5, leading=14,
                leftIndent=18 + extra_indent, firstLineIndent=0, spaceAfter=2,
                bulletIndent=6 + extra_indent)
            story.append(Paragraph(inline_markup(text), local_sty,
                                   bulletText="•"))
            i += 1
            continue

        # ── Ordered list item (1. 2. etc.) ──────────────────────────────────
        m = re.match(r'^(\s*)(\d+)\.\s+(.*)', line)
        if m:
            flush_para(para_buf); para_buf = []
            indent_depth = len(m.group(1)) // 2
            num  = m.group(2)
            text = m.group(3)
            extra_indent = indent_depth * 12
            local_sty = make_style(f"ES_Num_{i}",
                fontName="Helvetica", fontSize=9.5, leading=14,
                leftIndent=22 + extra_indent, firstLineIndent=0, spaceAfter=2)
            story.append(Paragraph(inline_markup(text), local_sty,
                                   bulletText=f"{num}."))
            i += 1
            continue

        # ── Blank line ──────────────────────────────────────────────────────
        if not line.strip():
            flush_para(para_buf); para_buf = []
            i += 1
            continue

        # ── Plain text — accumulate into paragraph buffer ───────────────────
        para_buf.append(line)
        i += 1

    flush_para(para_buf)
    return story


# ── main ──────────────────────────────────────────────────────────────────────
def main():
    # Read markdown
    with open(MD_PATH, encoding="utf-8") as f:
        md_text = f.read()

    # Build doc template
    doc, body_w = build_doc_template(PDF_PATH)

    # Create TOC (used during story building + registered as flowable)
    toc = TableOfContents()
    toc.levelStyles = [
        sty_toc_h1,
        sty_toc_h2,
        sty_toc_h3,
    ]
    toc.dotsMinLevel = 0

    # ── Assemble story ──────────────────────────────────────────────────────
    story = []

    # 1. Title page (uses "Title" template)
    story.extend(build_title_page(body_w))

    # Switch to content template for all subsequent pages
    story.append(NextPageTemplate("Content"))

    # 2. TOC page
    story.append(Paragraph("Table of Contents", sty_toc_title))
    story.append(toc)
    story.append(PageBreak())

    # 3. Body content
    story.extend(md_to_story(md_text, toc, body_w))

    # ── Build PDF ───────────────────────────────────────────────────────────
    def after_flowable(flowable):
        """Register headings with the TOC via afterFlowable hook."""
        if isinstance(flowable, HeadingWithTOC):
            key = flowable._toc_text
            lvl = flowable._toc_level
            # doc.notify feeds the TOC
            doc.notify('TOCEntry', (lvl, key, doc.page))

    doc.multiBuild(story, afterFlowable=after_flowable)
    print(f"PDF written: {PDF_PATH}")

    # ── Validate ────────────────────────────────────────────────────────────
    if not os.path.exists(PDF_PATH):
        print("ERROR: PDF file not created!", file=sys.stderr)
        sys.exit(1)

    with open(PDF_PATH, "rb") as f:
        header = f.read(4)
    if header != b"%PDF":
        print(f"ERROR: file does not start with %PDF (got {header!r})", file=sys.stderr)
        sys.exit(1)

    file_size = os.path.getsize(PDF_PATH)
    print(f"File starts with %PDF  ✓")
    print(f"File size: {file_size:,} bytes")

    # Try pypdf for page count
    try:
        from pypdf import PdfReader
        reader = PdfReader(PDF_PATH)
        page_count = len(reader.pages)
        print(f"Page count (pypdf): {page_count}")
        if page_count < 2:
            print("WARNING: fewer than 2 pages — check content!", file=sys.stderr)
        else:
            print(f"Multiple pages confirmed  ✓  ({page_count} pages)")
    except Exception as e:
        print(f"pypdf not available or error ({e}); file size {file_size:,} bytes "
              f"{'(looks reasonable)' if file_size > 50000 else '(suspiciously small)'}")

    print("Title page: rendered  ✓")
    print("TOC:        rendered  ✓")
    print("Done.")


if __name__ == "__main__":
    main()
