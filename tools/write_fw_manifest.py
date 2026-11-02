"""Write version.json next to the firmware images served by the frame firmware server.

    python3 tools/write_fw_manifest.py build/fw-images --version v2026.1.0 [--commit SHA] ...

"version" is the same UC2_FW_VERSION string the images report on the device
(/state_get "identifier_version", CANopen OD 0x2500 / bus-scan "fwVersion"), so
ImSwitch can tell whether a board runs what the server offers. "files" lists
every .bin with its size and sha256 so a download can be verified before
flashing. Only .bin files written by this build are listed; a file dropped into
the server by hand has no entry and therefore no known version.
"""
import argparse
import hashlib
import json
from pathlib import Path


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("image_dir", type=Path)
    p.add_argument("--version", required=True)
    p.add_argument("--commit", default="")
    p.add_argument("--commit-time", default="")
    p.add_argument("--run-url", default="")
    a = p.parse_args()

    files = {
        f.name: {"size": f.stat().st_size, "sha256": hashlib.sha256(f.read_bytes()).hexdigest()}
        for f in sorted(a.image_dir.glob("*.bin"))
    }
    manifest = {
        "version": a.version,
        "commit": a.commit,
        "commit_time": a.commit_time,
        "run_url": a.run_url,
        "files": files,
    }
    (a.image_dir / "version.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"version.json: {a.version}, {len(files)} images")


if __name__ == "__main__":
    main()
