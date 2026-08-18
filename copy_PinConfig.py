import shutil
import os
Import("env")

def before_build():
    scf = env.get("PIOENV", "")
    scf = scf.replace("_debug", "")
    scf = scf.replace("_release", "")
    source_file = f"main/config/{scf}/PinConfig.h"
    destination_file = "main/PinConfig.h"
    if not os.path.isfile(source_file):
        raise FileNotFoundError(f"PinConfig source not found: {source_file}")
    shutil.copy2(source_file, destination_file)
    print(f"Copied {source_file} -> {destination_file}")

before_build()
