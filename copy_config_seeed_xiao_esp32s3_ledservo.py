import shutil
import os

def before_build():
    print("Executing pre-build script...")  # This will print to the PlatformIO build log
    source_file = "main/config/seeed_xiao_esp32s3_ledservo/PinConfig.h"
    destination_file = "main/PinConfig.h"
    shutil.copy(source_file, destination_file)
    print(f"Copied {source_file} to {destination_file}")

# Register the before_build function as a pre-build action
before_build()
