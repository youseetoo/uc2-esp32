"""
PlatformIO extra_script — adds a custom 'canopen-regen' target.

Wire this up in platformio.ini by appending it to extra_scripts in
[env:esp32_framework] (or whichever base env you use):

    extra_scripts =
        pre:copy_PinConfig.py
        tools/canopen/canopen_targets.py

Then run:
    ~/.platformio/penv/bin/pio run -t canopen-regen

This is equivalent to:
    python tools/canopen/regenerate_all.py
"""

import subprocess
import sys
from pathlib import Path

Import("env")  # noqa: F821 — injected by SCons/PlatformIO


def canopen_regen(source, target, env):
    generator = Path(env["PROJECT_DIR"]) / "tools" / "canopen" / "regenerate_all.py"
    result = subprocess.run([sys.executable, str(generator)], cwd=env["PROJECT_DIR"])
    if result.returncode != 0:
        raise SystemExit(f"canopen-regen failed (exit {result.returncode})")


env.AddCustomTarget(  # noqa: F821
    name="canopen-regen",
    dependencies=None,
    actions=[canopen_regen],
    title="CANopen Regen",
    description="Regenerate all CANopen artifacts from uc2_canopen_registry.yaml",
    always_build=True,
)
