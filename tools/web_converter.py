#!/usr/bin/env python3
"""
Converts files from websrc/ to data/ before PlatformIO builds.
It also applies simple token replacement like {{APP_VERSION}}.
"""

from __future__ import annotations

from pathlib import Path
import shutil
import sys


TEXT_EXTENSIONS = {".html", ".css", ".js", ".json", ".txt", ".svg"}


def read_version(project_dir: Path) -> str:
    version_file = project_dir / "version.txt"
    if not version_file.exists():
        return "dev"
    return version_file.read_text(encoding="utf-8", errors="replace").strip() or "dev"


def ensure_clean_output(output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    for path in output_dir.rglob("*"):
        if path.is_file():
            path.unlink()


def convert(project_dir: Path) -> int:
    source_dir = project_dir / "websrc"
    output_dir = project_dir / "data"
    version = read_version(project_dir)

    if not source_dir.exists():
        print(f"[web-converter] Source folder not found: {source_dir}")
        return 1

    ensure_clean_output(output_dir)
    converted = 0

    for source_file in sorted(source_dir.rglob("*")):
        if not source_file.is_file():
            continue

        relative = source_file.relative_to(source_dir)
        target_file = output_dir / relative
        target_file.parent.mkdir(parents=True, exist_ok=True)

        if source_file.suffix.lower() in TEXT_EXTENSIONS:
            text = source_file.read_text(encoding="utf-8", errors="replace")
            text = text.replace("{{APP_VERSION}}", version)
            target_file.write_text(text, encoding="utf-8")
        else:
            shutil.copy2(source_file, target_file)

        converted += 1
        print(f"[web-converter] {relative}")

    print(f"[web-converter] Converted {converted} file(s), version={version}")
    return 0


def main() -> int:
    project_dir = Path(__file__).resolve().parent.parent
    return convert(project_dir)


if __name__ == "__main__":
    sys.exit(main())
