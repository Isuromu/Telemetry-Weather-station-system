"""Export the PCB review Markdown documents to readable, linked A4 PDFs.

This is a documentation-only export; it never modifies the Markdown sources.
"""

from __future__ import annotations

import html
import re
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import A4, landscape
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    BaseDocTemplate, Frame, PageTemplate, Paragraph, Preformatted,
    Spacer, LongTable, TableStyle, KeepTogether, CondPageBreak,
)
from pypdf import PdfReader


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "output" / "pdf"
SOURCES = [
    "SOIL_NODE_PCB_TECHNICAL_SPEC_DRAFT.md",
    "UNIVERSAL_12V_CONTROLLER_PCB_TECHNICAL_SPEC_DRAFT.md",
    "SOIL_NODE_ESP32C6_PINOUT_RU.md",
    "UNIVERSAL_12V_ESP32S3_PINOUT_RU.md",
]
PAPER = landscape(A4)
WIDTH = PAPER[0] - 84
INK = colors.HexColor("#203340")
TEAL = colors.HexColor("#126973")
LINE = colors.HexColor("#CFDBDF")


def fonts() -> None:
    folder = Path("C:/Windows/Fonts")
    for name, filename in [
        ("ArialDoc", "arial.ttf"), ("ArialDoc-Bold", "arialbd.ttf"),
        ("ArialDoc-Italic", "ariali.ttf"), ("ArialDoc-BoldItalic", "arialbi.ttf"),
        ("ConsolasDoc", "consola.ttf"),
    ]:
        pdfmetrics.registerFont(TTFont(name, str(folder / filename)))
    pdfmetrics.registerFontFamily(
        "ArialDoc", normal="ArialDoc", bold="ArialDoc-Bold",
        italic="ArialDoc-Italic", boldItalic="ArialDoc-BoldItalic",
    )


def clean(text: str) -> str:
    return text.translate(str.maketrans({
        "\u2010": "-", "\u2011": "-", "\u2012": "-",
        "\u2013": "-", "\u2014": "-", "\u2212": "-", "\u00a0": " ",
    }))


def link_target(target: str) -> str:
    if target.startswith(("https://", "http://", "mailto:")):
        return target
    filename, separator, anchor = target.partition("#")
    if filename in SOURCES:
        return Path(filename).with_suffix(".pdf").name
    return "../../docs/" + filename + (separator + anchor if separator else "")


INLINE = re.compile(r"\[([^\]]+)\]\(([^)]+)\)|`([^`]+)`|\*\*([^*]+)\*\*")


def inline(text: str, header: bool = False) -> str:
    text = clean(text)
    pieces = []
    position = 0
    for match in INLINE.finditer(text):
        pieces.append(html.escape(text[position:match.start()]))
        if match.group(1) is not None:
            color = "#FFFFFF" if header else "#126973"
            pieces.append(
                f'<link href="{html.escape(link_target(match.group(2)), quote=True)}" '
                f'color="{color}">{html.escape(match.group(1))}</link>'
            )
        elif match.group(3) is not None:
            # Use the proportional face in tables so long net names stay legible.
            pieces.append(f'<b>{html.escape(match.group(3))}</b>')
        else:
            pieces.append(f'<b>{html.escape(match.group(4))}</b>')
        position = match.end()
    pieces.append(html.escape(text[position:]))
    return "".join(pieces)


def styles() -> dict[str, ParagraphStyle]:
    body = ParagraphStyle(
        "Body", fontName="ArialDoc", fontSize=10, leading=14,
        textColor=INK, spaceAfter=7, splitLongWords=True,
        allowWidows=0, allowOrphans=0,
    )
    return {
        "body": body,
        "title": ParagraphStyle("Title", parent=body, fontName="ArialDoc-Bold",
                                fontSize=21, leading=26, spaceAfter=15),
        "h2": ParagraphStyle("H2", parent=body, fontName="ArialDoc-Bold",
                             fontSize=14, leading=18, textColor=TEAL,
                             spaceBefore=14, spaceAfter=8, keepWithNext=True),
        "h3": ParagraphStyle("H3", parent=body, fontName="ArialDoc-Bold",
                             fontSize=11.3, leading=15, spaceBefore=10,
                             spaceAfter=6, keepWithNext=True),
        "list": ParagraphStyle("List", parent=body, leftIndent=14,
                               firstLineIndent=0, bulletIndent=1, spaceAfter=4,
                               bulletFontName="ArialDoc", bulletFontSize=9),
        "cell": ParagraphStyle("Cell", parent=body, fontSize=8.5, leading=11.2,
                               spaceAfter=0, allowWidows=1, allowOrphans=1),
        "thead": ParagraphStyle("THead", parent=body, fontName="ArialDoc-Bold",
                                fontSize=8.4, leading=11, textColor=colors.white,
                                spaceAfter=0, allowWidows=1, allowOrphans=1),
        "code": ParagraphStyle("Code", fontName="ConsolasDoc", fontSize=8.4,
                               leading=11.2, textColor=INK, spaceAfter=10,
                               backColor=colors.HexColor("#F0F5F6"),
                               borderPadding=9, leftIndent=8, rightIndent=8),
    }


def table_widths(rows: list[list[str]]) -> list[float]:
    header = rows[0]
    count = len(header)
    if count == 6 and "Module pad" in header:
        weights = [0.05, 0.075, 0.205, 0.05, 0.31, 0.31]
    elif count == 5 and "Площадка модуля" in header:
        weights = [0.055, 0.09, 0.05, 0.285, 0.52]
    elif count == 5 and ("RGJ package pin" in header or "Ножка RGJ" in header):
        weights = [0.07, 0.08, 0.30, 0.33, 0.22] if "Ножка RGJ" in header else [0.075, 0.10, 0.35, 0.06, 0.415]
    else:
        lengths = [max(len(clean(row[i])) for row in rows) for i in range(count)]
        weights = [max(5, min(90, length)) ** 0.65 for length in lengths]
        total = sum(weights)
        weights = [weight / total for weight in weights]
    return [WIDTH * value / sum(weights) for value in weights]


def make_table(rows: list[list[str]], sheet: dict) -> LongTable:
    count = len(rows[0])
    if any(len(row) != count for row in rows):
        raise ValueError(f"Inconsistent table columns: {rows}")
    content = [[Paragraph(inline(value, index == 0), sheet["thead" if index == 0 else "cell"])
                for value in row] for index, row in enumerate(rows)]
    obj = LongTable(content, colWidths=table_widths(rows), repeatRows=1,
                    hAlign="LEFT", splitByRow=1, spaceBefore=4, spaceAfter=11,
                    rowSplitRange=(2, len(rows) - 2) if len(rows) > 4 else None)
    obj.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), TEAL),
        ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#F1F5F6")]),
        ("LINEBELOW", (0, 0), (-1, 0), 0.7, TEAL),
        ("LINEBELOW", (0, 1), (-1, -1), 0.3, LINE),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 6),
        ("RIGHTPADDING", (0, 0), (-1, -1), 6),
        ("TOPPADDING", (0, 0), (-1, -1), 6),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 6),
    ]))
    return obj


def special(line: str) -> bool:
    return bool(re.match(r"^(#{1,6} |```|\| |[-*] |\d+\. |>)", line))


def parse_markdown(source: str, sheet: dict) -> list:
    lines = source.splitlines()
    story = []
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if not line:
            i += 1
            continue
        if line.startswith("```"):
            i += 1
            code = []
            while i < len(lines) and not lines[i].strip().startswith("```"):
                code.append(clean(lines[i]))
                i += 1
            longest = max((pdfmetrics.stringWidth(row, "ConsolasDoc", 8.4) for row in code), default=1)
            code_style = ParagraphStyle("CodeFit", parent=sheet["code"])
            if longest > WIDTH - 34:
                code_style.fontSize = 8.4 * (WIDTH - 34) / longest
                code_style.leading = code_style.fontSize * 1.34
            story.append(Preformatted("\n".join(code), code_style))
            i += 1
            continue
        heading = re.match(r"^(#{1,6})\s+(.+)$", line)
        if heading:
            level = len(heading.group(1))
            style = "title" if level == 1 else "h2" if level == 2 else "h3"
            paragraph = Paragraph(inline(heading.group(2)), sheet[style])
            if level == 2:
                paragraph.outline_title = clean(heading.group(2))
            story.append(paragraph)
            i += 1
            continue
        if line.startswith("|"):
            rows = []
            while i < len(lines) and lines[i].strip().startswith("|"):
                row = [cell.strip() for cell in lines[i].strip().strip("|").split("|")]
                if not all(re.fullmatch(r":?-{2,}:?", cell) for cell in row):
                    rows.append(row)
                i += 1
            story.append(make_table(rows, sheet))
            continue
        bullet = re.match(r"^([-*]|\d+\.)\s+(.+)$", line)
        if bullet:
            marker = "-" if bullet.group(1) in ("-", "*") else bullet.group(1)
            block = [bullet.group(2)]
            i += 1
            while i < len(lines) and lines[i].strip() and not special(lines[i].strip()):
                block.append(lines[i].strip())
                i += 1
            story.append(Paragraph(inline(" ".join(block)), sheet["list"], bulletText=marker))
            continue
        block = [line.lstrip("> ") if line.startswith(">") else line]
        i += 1
        while i < len(lines) and lines[i].strip() and not special(lines[i].strip()):
            block.append(lines[i].strip())
            i += 1
        story.append(Paragraph(inline(" ".join(block)), sheet["body"]))
    return story


def prepare_layout(story: list) -> list:
    """Keep small connector tables intact without pushing huge tables whole."""
    result = []
    for item in story:
        if isinstance(item, LongTable):
            _, height = item.wrap(WIDTH, 10000)
            previous = result[-1] if result else None
            if height <= 250:
                if isinstance(previous, Paragraph) and previous.style.name != "List":
                    previous.keepWithNext = True
                result.append(KeepTogether([item]))
                continue
            if isinstance(previous, Paragraph) and previous.style.name in ("H2", "H3"):
                previous.keepWithNext = False
                result.insert(len(result) - 1, CondPageBreak(110))
        result.append(item)
    return result


class ReviewPDF(BaseDocTemplate):
    def __init__(self, target: Path, label: str, title: str):
        super().__init__(str(target), pagesize=PAPER, leftMargin=42, rightMargin=42,
                         topMargin=43, bottomMargin=39, title=title,
                         author="AmudarIO - PCB review documentation",
                         pageCompression=1)
        self.label = label
        self.bookmark_index = 0
        self.addPageTemplates(PageTemplate(
            id="review", frames=[Frame(42, 39, WIDTH, PAPER[1] - 82,
                                        leftPadding=0, rightPadding=0,
                                        topPadding=0, bottomPadding=0)],
            onPage=self.decorate,
        ))

    def decorate(self, canvas, doc):
        canvas.saveState()
        canvas.setStrokeColor(LINE)
        canvas.setLineWidth(0.5)
        canvas.line(42, PAPER[1] - 29, PAPER[0] - 42, PAPER[1] - 29)
        canvas.setFont("ArialDoc-Bold", 8)
        canvas.setFillColor(TEAL)
        canvas.drawString(42, PAPER[1] - 20, "AMUDARIO / PCB REVIEW")
        canvas.setFont("ArialDoc", 8)
        canvas.setFillColor(INK)
        canvas.drawRightString(PAPER[0] - 42, PAPER[1] - 20, self.label)
        canvas.setStrokeColor(LINE)
        canvas.line(42, 28, PAPER[0] - 42, 28)
        canvas.setFont("ArialDoc", 7.5)
        canvas.drawString(42, 17, "Review draft - not a released schematic or manufacturing package")
        canvas.drawRightString(PAPER[0] - 42, 17, str(doc.page))
        canvas.restoreState()

    def afterFlowable(self, flowable):
        if hasattr(flowable, "outline_title"):
            self.bookmark_index += 1
            key = f"section-{self.bookmark_index}"
            self.canv.bookmarkPage(key)
            self.canv.addOutlineEntry(flowable.outline_title, key, 0, False)


def verify_pdf(source: str, output: Path) -> tuple[int, int]:
    pdf = PdfReader(str(output))
    extracted = "\n".join(page.extract_text() or "" for page in pdf.pages)
    compact = re.sub(r"\s+", "", clean(extracted))
    # Check every heading and every exact net label enclosed in code spans.
    required = [clean(m.group(2)) for m in re.finditer(r"^(#{1,3})\s+(.+)$", source, re.M)]
    required += re.findall(r"`([A-Z][A-Z0-9_]+)`", source)
    missing = [value for value in required if re.sub(r"\s+", "", value) not in compact]
    if missing:
        raise AssertionError(f"Missing content in {output.name}: {missing}")
    if "\ufffd" in extracted:
        raise AssertionError(f"Replacement glyph in {output.name}")
    return len(pdf.pages), len(required)


def main() -> None:
    fonts()
    OUT.mkdir(parents=True, exist_ok=True)
    sheet = styles()
    for filename in SOURCES:
        source = (ROOT / "docs" / filename).read_text(encoding="utf-8")
        target = OUT / Path(filename).with_suffix(".pdf").name
        title = clean(source.splitlines()[0].lstrip("# "))
        is_soil = filename.startswith("SOIL")
        is_ru = filename.endswith("_RU.md")
        label = ("Soil / ESP32-C6" if is_soil else "Universal 12 V / ESP32-S3")
        label += " / Pinout RU" if is_ru else " / Technical specification"
        ReviewPDF(target, label, title).build(prepare_layout(parse_markdown(source, sheet)))
        pages, checks = verify_pdf(source, target)
        print(f"PASS {target.name}: {pages} pages, {checks} content checks, {target.stat().st_size} bytes")


if __name__ == "__main__":
    main()
