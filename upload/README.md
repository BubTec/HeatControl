# Embedded Web Files

This folder contains web assets that are embedded into firmware during build.

## Workflow

1. Edit files in `upload/` (including `upload/version.txt`).
2. Build firmware as usual (`platformio run -e esp32-c3`).
3. `embed_webfiles.py` runs automatically via `extra_script.py`.

Generated files are written to `src/generated/` and are not committed.
