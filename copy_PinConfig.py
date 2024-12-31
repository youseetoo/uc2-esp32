import shutil
import os
Import("env")
print(env.get("PIOENV",[]))

def before_build():
    print("Executing pre-build script...")  # This will print to the PlatformIO build log
    scf = env.get("PIOENV",[])
    scf = scf.replace("_debug","")
    print(scf)
    try:
        source_file = "main/config/"+ scf +"/PinConfig.h"
        destination_file = "main/PinConfig.h"
        shutil.copy(source_file, destination_file)
        print(f"Copied {source_file} to {destination_file}")
    except Exception as e:
        print("Error, not copied")
        print(e)
# Register the before_build function as a pre-build action
before_build()
