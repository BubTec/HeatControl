#!/usr/bin/env python3
"""Generate embedded web resources from upload/ into src/generated/."""

from __future__ import annotations

import hashlib
from pathlib import Path
import re
import sys


CONTENT_TYPES = {
    ".html": "text/html",
    ".css": "text/css",
    ".js": "application/javascript",
    ".json": "application/json",
    ".svg": "image/svg+xml",
    ".txt": "text/plain",
    ".ico": "image/x-icon",
    ".png": "image/png",
    ".jpg": "image/jpeg",
    ".jpeg": "image/jpeg",
}


def content_type(path: Path) -> str:
    return CONTENT_TYPES.get(path.suffix.lower(), "application/octet-stream")


def sanitize_symbol(relative_path: str) -> str:
    name = re.sub(r"[^0-9a-zA-Z_]", "_", relative_path)
    if name and name[0].isdigit():
        name = f"f_{name}"
    return name.lower()


def bytes_to_cpp_array(data: bytes) -> str:
    lines = []
    for i in range(0, len(data), 16):
        chunk = data[i : i + 16]
        line = ", ".join(f"0x{b:02X}" for b in chunk)
        lines.append(f"    {line}")
    return ",\n".join(lines)


def generate_single_header(source_file: Path, source_root: Path, out_dir: Path) -> dict:
    rel = source_file.relative_to(source_root).as_posix()
    symbol = sanitize_symbol(rel)
    data = source_file.read_bytes()
    md5sum = hashlib.md5(data).hexdigest()[:8]

    header = f"""// Auto-generated from {rel}
#pragma once

#include <Arduino.h>

const uint8_t embedded_{symbol}_data[] PROGMEM = {{
{bytes_to_cpp_array(data)}
}};

constexpr size_t embedded_{symbol}_size = {len(data)};
constexpr const char *embedded_{symbol}_path = "/{rel}";
constexpr const char *embedded_{symbol}_content_type = "{content_type(source_file)}";
constexpr const char *embedded_{symbol}_checksum = "{md5sum}";
"""

    target = out_dir / f"embedded_{symbol}.h"
    target.write_text(header, encoding="utf-8")

    return {
        "symbol": symbol,
        "path": f"/{rel}",
        "content_type": content_type(source_file),
    }


def generate_registry(files: list[dict], out_dir: Path) -> None:
    includes = "\n".join(f'#include "embedded_{f["symbol"]}.h"' for f in files)

    registry_h = f"""// Auto-generated embedded file registry
#pragma once

#include <Arduino.h>

{includes}

struct EmbeddedFile {{
    const char *path;
    const char *content_type;
    const uint8_t *data;
    size_t size;
}};

extern const EmbeddedFile embedded_files[];
extern const size_t embedded_files_count;
const EmbeddedFile *findEmbeddedFile(const char *path);
"""

    rows = "\n".join(
        "    {"
        f' "{f["path"]}",'
        f' "{f["content_type"]}",'
        f" embedded_{f['symbol']}_data,"
        f" embedded_{f['symbol']}_size"
        " },"
        for f in files
    )

    registry_cpp = f"""// Auto-generated embedded file registry implementation
#include "embedded_files_registry.h"

#include <cstring>

const EmbeddedFile embedded_files[] = {{
{rows}
}};

const size_t embedded_files_count = sizeof(embedded_files) / sizeof(embedded_files[0]);

const EmbeddedFile *findEmbeddedFile(const char *path) {{
    if (path == nullptr) {{
        return nullptr;
    }}
    for (size_t i = 0; i < embedded_files_count; ++i) {{
        if (std::strcmp(embedded_files[i].path, path) == 0) {{
            return &embedded_files[i];
        }}
    }}
    return nullptr;
}}
"""

    (out_dir / "embedded_files_registry.h").write_text(registry_h, encoding="utf-8")
    (out_dir / "embedded_files_registry.cpp").write_text(registry_cpp, encoding="utf-8")


def clean_old_generated(out_dir: Path) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    for file in out_dir.glob("embedded_*"):
        if file.is_file():
            file.unlink()


def main() -> int:
    project_dir = Path(__file__).resolve().parent
    source_root = project_dir / "upload"
    out_dir = project_dir / "src" / "generated"

    if not source_root.exists():
        print(f"[embed] Source folder missing: {source_root}")
        return 1

    clean_old_generated(out_dir)
    generated = []

    for source_file in sorted(source_root.rglob("*")):
        if not source_file.is_file():
            continue
        rel = source_file.relative_to(source_root).as_posix()
        if source_file.name.startswith(".") or source_file.suffix.lower() in {".md", ".bin"}:
            continue
        generated.append(generate_single_header(source_file, source_root, out_dir))
        print(f"[embed] {rel}")

    if not generated:
        print("[embed] No files found in upload/")
        return 1

    generate_registry(generated, out_dir)
    print(f"[embed] Generated {len(generated)} embedded file(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
