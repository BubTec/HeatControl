Import("env")

import os
import shutil

def copy_unity_config(env):
    """Copy unity_config.h to Unity library directory for native tests."""
    if env["PIOENV"] != "native":
        return
    
    unity_lib_path = os.path.join(env["PROJECT_LIBDEPS_DIR"], "native", "Unity", "src")
    config_source = os.path.join(env["PROJECT_DIR"], "test", "unity_config.h")
    config_dest = os.path.join(unity_lib_path, "unity_config.h")
    
    if os.path.exists(config_source):
        os.makedirs(unity_lib_path, exist_ok=True)
        shutil.copy2(config_source, config_dest)
        print(f"[native] Copied unity_config.h to {config_dest}")

copy_unity_config(env)
