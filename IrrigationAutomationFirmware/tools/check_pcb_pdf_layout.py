"""Render every exported PCB PDF page and build visual-QA contact sheets."""

from pathlib import Path
import json
import math
import shutil
import subprocess

from PIL import Image, ImageDraw, ImageFont
from pypdf import PdfReader

from export_pcb_review_pdfs import ROOT, OUT, SOURCES


def main() -> None:
    renderer = shutil.which("pdftoppm")
    if not renderer:
        raise RuntimeError("Poppler pdftoppm is required for visual QA")
    qa = ROOT / "tmp" / "pdfs" / "pcb_export_qa_final"
    qa.mkdir(parents=True, exist_ok=True)
    font = ImageFont.truetype("C:/Windows/Fonts/arial.ttf", 16)
    report = []
    for source in SOURCES:
        stem = Path(source).stem
        folder = qa / stem
        folder.mkdir(exist_ok=True)
        pdf = OUT / f"{stem}.pdf"
        subprocess.run([renderer, "-r", "65", "-png", str(pdf), str(folder / "page")], check=True)
        pages = sorted(folder.glob("page-*.png"), key=lambda p: int(p.stem.split("-")[-1]))
        reader = PdfReader(str(pdf))
        if len(pages) != len(reader.pages):
            raise AssertionError("Not every page was rendered")
        sheets = []
        for start in range(0, len(pages), 9):
            group = pages[start:start + 9]
            tile_w, tile_h = 650, 480
            sheet = Image.new("RGB", (tile_w * 3, tile_h * math.ceil(len(group) / 3)), "#dce5e9")
            draw = ImageDraw.Draw(sheet)
            for offset, path in enumerate(group):
                picture = Image.open(path).convert("RGB")
                picture.thumbnail((630, 446), Image.Resampling.LANCZOS)
                x, y = (offset % 3) * tile_w, (offset // 3) * tile_h
                sheet.paste(picture, (x + 10, y + 25))
                draw.text((x + 12, y + 4), f"Page {start + offset + 1}", font=font, fill="#203340")
            result = folder / f"contact-{start // 9 + 1}.png"
            sheet.save(result)
            sheets.append(str(result))
        report.append({"file": str(pdf), "pages": len(pages), "contact_sheets": sheets})
    (qa / "render_report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
