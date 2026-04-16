from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent
HEADER_FILE = (BASE_DIR / "../header.txt").resolve()
TARGET_DIR = (BASE_DIR / "..").resolve()

TARGET_EXT = {".cpp", ".hpp"}
LINES_TO_REMOVE = 8  # 削除する行数


def load_header():
    """ヘッダファイルを読み込む"""
    return HEADER_FILE.read_text(encoding="utf-8").rstrip() + "\n\n"


def process_file(path: Path, header: str):
    """ファイルの最初の8行を削除し、必要に応じてヘッダを追加する"""
    try:
        original_lines = path.read_text(encoding="utf-8").splitlines()
        # 最初のLINES_TO_REMOVE行を削除
        remaining_lines = original_lines[LINES_TO_REMOVE:]

        # ヘッダをまだ追加していなければ追加
        if header.strip() not in "\n".join(remaining_lines[:20]):
            remaining_lines = header.splitlines() + remaining_lines

        # ファイルに書き込み
        path.write_text("\n".join(remaining_lines) + "\n", encoding="utf-8")
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