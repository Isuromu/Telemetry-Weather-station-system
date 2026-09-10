"""Run the document skill renderer with installed Word as its PDF backend."""
from pathlib import Path
import importlib.util
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SKILL = Path('C:/Users/user/.codex/plugins/cache/openai-primary-runtime/documents/26.903.11726/skills/documents')
spec = importlib.util.spec_from_file_location('render_docx', SKILL / 'render_docx.py')
renderer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(renderer)

def word_pdf(doc_path, user_profile, convert_tmp_dir, stem, verbose):
    target = str(Path(convert_tmp_dir) / (stem + '.pdf'))
    proc = subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(ROOT / 'tools' / 'export_docx_with_word.ps1'), '-InputDocx', doc_path, '-OutputPdf', target], capture_output=True, text=True, timeout=120)
    if proc.returncode or not Path(target).exists():
        raise RuntimeError(proc.stdout + proc.stderr)
    return target, 'Microsoft Word PDF export\n' + proc.stdout

renderer.convert_to_pdf = word_pdf
if __name__ == '__main__':
    renderer.main()
