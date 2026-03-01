import sys
try:
    import pypdf
    from pypdf import PdfReader
    
    reader = PdfReader(sys.argv[1])
    text = ""
    for page in reader.pages:
        text += page.extract_text() + "\n"
    
    with open(sys.argv[2], "w", encoding="utf-8") as f:
        f.write(text)
    print("Success")
except ImportError:
    print("pypdf not installed.")
except Exception as e:
    print(f"Error: {e}")
