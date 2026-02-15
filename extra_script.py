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


def run_prebuild(source, target, env):
    run_script(env, "web-converter", os.path.join("tools", "web_converter.py"))
    run_script(env, "embed", "embed_webfiles.py")


# Run once at script load so generated C++ exists before dependency scanning.
run_prebuild(None, None, env)

env.AddPreAction("buildprog", run_prebuild)
env.AddPreAction("buildfs", run_prebuild)
