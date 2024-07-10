import shutil
import os

def before_build(source, target, env):
    source_file = "main/config/UC2_3/PinConfig.h"
    destination_file = "main/PinConfig.h"
    shutil.copy(source_file, destination_file)
    print(f"Copied {source_file} to {destination_file}")

# Register the before_build function as a pre-build action
env.AddPreAction("buildprog", before_build)
