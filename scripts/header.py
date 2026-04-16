from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent
HEADER_FILE = (BASE_DIR / "../header.txt").resolve()
TARGET_DIR = (BASE_DIR / "..").resolve()

TARGET_EXT = {".cpp", ".hpp"}


def load_header():
    return HEADER_FILE.read_text(encoding="utf-8").rstrip() + "\n\n"


def process_file(path: Path, header: str):
    try:
        original = path.read_text(encoding="utf-8")

        if header.strip() in original[:1000]:
            return

        path.write_text(header + original, encoding="utf-8")
        print(f"Updated: {path}")

    except Exception as e:
        print(f"Failed: {path} ({e})")


def main():
    header = load_header()

    for file in TARGET_DIR.rglob("*"):
        if file.suffix in TARGET_EXT:
            process_file(file, header)


if __name__ == "__main__":
    main()