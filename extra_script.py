Import("env")

import os
import subprocess
import sys


def run_script(env, tag, relative_script):
    project_dir = env.get("PROJECT_DIR")
    script_path = os.path.join(project_dir, relative_script)

    if not os.path.exists(script_path):
        print(f"[{tag}] Missing script: {script_path}")
        env.Exit(1)

    print(f"[{tag}] Running pre-build step")
    result = subprocess.run(
        [sys.executable, script_path],
        cwd=project_dir,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )

    if result.stdout:
        print(result.stdout)
    if result.stderr:
        print(result.stderr)

    if result.returncode != 0:
        print(f"[{tag}] Step failed with exit code {result.returncode}")
        env.Exit(result.returncode)


def patch_onewire_warning(env):
    project_dir = env.get("PROJECT_DIR")
    pio_env = env.get("PIOENV")
    if not pio_env:
        return

    onewire_cpp = os.path.join(
        project_dir, ".pio", "libdeps", pio_env, "OneWire", "OneWire.cpp"
    )
    if not os.path.exists(onewire_cpp):
        return

    with open(onewire_cpp, "r", encoding="utf-8", errors="replace") as f:
        content = f.read()

    updated = content.replace(
        "#  undef noInterrupts() {portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;portENTER_CRITICAL(&mux)",
        "#  undef noInterrupts",
    ).replace(
        "#  undef interrupts() portEXIT_CRITICAL(&mux);}",
        "#  undef interrupts",
    )

    if updated != content:
        with open(onewire_cpp, "w", encoding="utf-8", newline="") as f:
            f.write(updated)
        print("[onewire-patch] Patched OneWire.cpp to remove invalid #undef tokens")


def run_prebuild(source, target, env):
    run_script(env, "embed", "embed_webfiles.py")
    patch_onewire_warning(env)


# Run once at script load so generated C++ exists before dependency scanning.
run_prebuild(None, None, env)

env.AddPreAction("buildprog", run_prebuild)
env.AddPreAction("buildfs", run_prebuild)
