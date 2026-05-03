#!/usr/bin/env python3
"""
Generate all CANopen artifacts from the central parameter registry.

Reads:  uc2_canopen_registry.yaml
Writes:
    DOCUMENTATION/openUC2_satellite.eds         (CiA 306 INI for CANopenEditor)
    lib/uc2_od/OD.h                              (CANopenNode v4 header)
    lib/uc2_od/OD.c                              (CANopenNode v4 source)
    main/src/canopen/UC2_OD_Indices.h            (C++ named constants)
    PYTHON/uc2_canopen/uc2_indices.py            (Python named constants)
    DOCUMENTATION/UC2_CANopen_Parameters.md      (human-readable reference)

Usage:
    python tools/regenerate_all.py [registry.yaml]

When you add a new parameter:
    1. Edit uc2_canopen_registry.yaml
    2. Run: python tools/regenerate_all.py
    3. Commit all generated files together
    4. Open in CANopenEditor to verify the EDS still loads cleanly

NOTE: The generated OD.h/OD.c are produced from a template that mirrors what
CANopenEditor would emit. CANopenEditor remains useful for VISUAL validation
and for round-trip editing — load the .eds, eyeball PDO mappings, save back.
But the source of truth is THIS registry, not the .eds.
"""

import sys
import os
import datetime
from pathlib import Path

try:
    import yaml
except ImportError:
    print("ERROR: PyYAML required. pip install pyyaml")
    sys.exit(1)


# ============================================================================
# Type system
# ============================================================================

# CiA 301 data type codes (for EDS)
EDS_TYPES = {
    "BOOL":   "0x0001",
    "I8":     "0x0002",
    "I16":    "0x0003",
    "I32":    "0x0004",
    "U8":     "0x0005",
    "U16":    "0x0006",
    "U32":    "0x0007",
    "STRING": "0x0009",
    "DOMAIN": "0x000F",
    "I64":    "0x0015",
    "U64":    "0x001B",
}

# C type names for OD.h
C_TYPES = {
    "BOOL":   "bool_t",
    "I8":     "int8_t",
    "I16":    "int16_t",
    "I32":    "int32_t",
    "U8":     "uint8_t",
    "U16":    "uint16_t",
    "U32":    "uint32_t",
    "STRING": "char",
    "DOMAIN": "uint8_t",   # treated as opaque buffer
    "I64":    "int64_t",
    "U64":    "uint64_t",
}

# Bit sizes for PDO mapping calculations
TYPE_BITS = {
    "BOOL": 1, "I8": 8, "U8": 8,
    "I16": 16, "U16": 16,
    "I32": 32, "U32": 32,
    "I64": 64, "U64": 64,
}


# ============================================================================
# Helpers
# ============================================================================

def load_registry(path):
    with open(path) as f:
        return yaml.safe_load(f)


def all_entries(registry):
    """Iterate over all (module_name, entry) pairs."""
    for mod_name, mod in registry["modules"].items():
        for entry in mod["entries"]:
            yield mod_name, entry


def entry_cpp_const(mod_name, entry):
    """Generate the C++ constant name for an entry: UC2_OD::MOTOR_TARGET_POSITION"""
    return entry["name"].upper()


# ============================================================================
# 1. EDS file (CiA 306 INI format) — for CANopenEditor
# ============================================================================

def generate_eds(registry, out_path):
    L = []
    w = lambda s="": L.append(s)

    now = datetime.datetime.now()
    dev = registry["device"]

    # File info
    w("[FileInfo]")
    w(f"FileName={Path(out_path).name}")
    w("FileVersion=1")
    w("FileRevision=1")
    w("EDSVersion=4.0")
    w(f"Description={dev['product_name']}")
    w(f"CreationTime={now.strftime('%I:%M%p')}")
    w(f"CreationDate={now.strftime('%m-%d-%Y')}")
    w(f"CreatedBy={dev['vendor_name']}")
    w(f"ModificationTime={now.strftime('%I:%M%p')}")
    w(f"ModificationDate={now.strftime('%m-%d-%Y')}")
    w(f"ModifiedBy={dev['vendor_name']}")
    w()

    w("[DeviceInfo]")
    w(f"VendorName={dev['vendor_name']}")
    w(f"VendorNumber=0x{dev['vendor_id']:08X}")
    w(f"ProductName={dev['product_name']}")
    w(f"ProductNumber=0x{dev['product_code']:08X}")
    w(f"RevisionNumber=0x{dev['revision']:08X}")
    for b in [10, 20, 50, 125, 250, 500, 800, 1000]:
        supported = 1 if b in dev['baudrates'] else 0
        w(f"BaudRate_{b}={supported}")
    w("SimpleBootUpMaster=0")
    w("SimpleBootUpSlave=1")
    w("Granularity=8")
    w("DynamicChannelsSupported=0")
    w("CompactPDO=0")
    w("GroupMessaging=0")
    w(f"NrOfRXPDO={sum(1 for k in registry['pdo_mapping'] if k.startswith('rpdo'))}")
    w(f"NrOfTXPDO={sum(1 for k in registry['pdo_mapping'] if k.startswith('tpdo'))}")
    w("LSS_Supported=1")
    w("NG_Slave=0")
    w()

    w("[DummyUsage]")
    w("Dummy0001=0")
    for i in range(2, 8):
        w(f"Dummy000{i}=1")
    w()

    w("[Comments]")
    w("Lines=2")
    w(f"Line1=Generated from uc2_canopen_registry.yaml on {now.strftime('%Y-%m-%d')}")
    w("Line2=DO NOT EDIT — regenerate with tools/regenerate_all.py")
    w()

    # Mandatory objects (CiA 301)
    w("[MandatoryObjects]")
    w("SupportedObjects=3")
    w("1=0x1000")
    w("2=0x1001")
    w("3=0x1018")
    w()

    # 0x1000 device type
    w("[1000]")
    w("ParameterName=Device type")
    w("ObjectType=0x7")
    w(";StorageLocation=PERSIST_COMM")
    w("DataType=0x0007")
    w("AccessType=ro")
    w("DefaultValue=0x00000000")
    w("PDOMapping=0")
    w()

    # 0x1001 error register
    w("[1001]")
    w("ParameterName=Error register")
    w("ObjectType=0x7")
    w(";StorageLocation=RAM")
    w("DataType=0x0005")
    w("AccessType=ro")
    w("DefaultValue=0x00")
    w("PDOMapping=1")
    w()

    # 0x1018 identity
    w("[1018]")
    w("ParameterName=Identity")
    w("ObjectType=0x9")
    w(";StorageLocation=PERSIST_COMM")
    w("SubNumber=0x5")
    w()
    for sub, name, val in [
        (0, "Highest sub-index supported", "0x04"),
        (1, "Vendor-ID", f"0x{dev['vendor_id']:08X}"),
        (2, "Product code", f"0x{dev['product_code']:08X}"),
        (3, "Revision number", f"0x{dev['revision']:08X}"),
        (4, "Serial number", "0x00000000"),
    ]:
        dt = "0x0005" if sub == 0 else "0x0007"
        w(f"[1018sub{sub}]")
        w(f"ParameterName={name}")
        w("ObjectType=0x7")
        w(";StorageLocation=PERSIST_COMM")
        w(f"DataType={dt}")
        w("AccessType=ro")
        w(f"DefaultValue={val}")
        w("PDOMapping=0")
        w()

    # Optional objects: heartbeat, SDO, PDO comm/mapping
    optional_indices = [0x1017, 0x1200]
    rpdo_count = 0
    tpdo_count = 0
    for k in registry["pdo_mapping"]:
        if k.startswith("rpdo"):
            optional_indices.append(0x1400 + rpdo_count)
            optional_indices.append(0x1600 + rpdo_count)
            rpdo_count += 1
        elif k.startswith("tpdo"):
            optional_indices.append(0x1800 + tpdo_count)
            optional_indices.append(0x1A00 + tpdo_count)
            tpdo_count += 1

    w("[OptionalObjects]")
    w(f"SupportedObjects={len(optional_indices)}")
    for i, idx in enumerate(optional_indices, 1):
        w(f"{i}=0x{idx:04X}")
    w()

    # 0x1017 heartbeat producer
    w("[1017]")
    w("ParameterName=Producer heartbeat time")
    w("ObjectType=0x7")
    w(";StorageLocation=PERSIST_COMM")
    w("DataType=0x0006")
    w("AccessType=rw")
    w("DefaultValue=500")
    w("PDOMapping=0")
    w()

    # 0x1200 SDO server
    w("[1200]")
    w("ParameterName=SDO server parameter")
    w("ObjectType=0x9")
    w(";StorageLocation=RAM")
    w("SubNumber=0x3")
    w()
    for sub, name, val in [
        (0, "Highest sub-index supported", "0x02"),
        (1, "COB-ID client to server (rx)", "$NODEID+0x600"),
        (2, "COB-ID server to client (tx)", "$NODEID+0x580"),
    ]:
        dt = "0x0005" if sub == 0 else "0x0007"
        w(f"[1200sub{sub}]")
        w(f"ParameterName={name}")
        w("ObjectType=0x7")
        w(";StorageLocation=RAM")
        w(f"DataType={dt}")
        w("AccessType=ro")
        w(f"DefaultValue={val}")
        w("PDOMapping=0")
        w()

    # PDO comm + mapping records
    rpdo_idx = 0
    tpdo_idx = 0
    for pdo_name, pdo in registry["pdo_mapping"].items():
        if pdo_name.startswith("rpdo"):
            comm_base = 0x1400 + rpdo_idx
            map_base = 0x1600 + rpdo_idx
            cob_default = f"$NODEID+{0x200 + 0x100*rpdo_idx:#x}"
            rpdo_idx += 1
        else:
            comm_base = 0x1800 + tpdo_idx
            map_base = 0x1A00 + tpdo_idx
            cob_default = f"$NODEID+{0x180 + 0x100*tpdo_idx:#x}"
            tpdo_idx += 1

        # Comm record
        w(f"[{comm_base:04X}]")
        w(f"ParameterName={pdo_name.upper()} communication parameter")
        w("ObjectType=0x9")
        w(";StorageLocation=PERSIST_COMM")
        w("SubNumber=0x6")
        w()
        for sub, name, dt, default in [
            (0, "Highest sub-index supported", "0x0005", "0x06"),
            (1, "COB-ID used by PDO", "0x0007", cob_default),
            (2, "Transmission type", "0x0005", str(pdo.get("transmission_type", 254))),
            (3, "Inhibit time", "0x0006", str(pdo.get("inhibit_time_ms", 0))),
            (5, "Event timer", "0x0006", str(pdo.get("event_timer_ms", 0))),
            (6, "SYNC start value", "0x0005", "0"),
        ]:
            w(f"[{comm_base:04X}sub{sub}]")
            w(f"ParameterName={name}")
            w("ObjectType=0x7")
            w(";StorageLocation=RAM")
            w(f"DataType={dt}")
            w("AccessType=rw" if sub != 0 else "AccessType=ro")
            w(f"DefaultValue={default}")
            w("PDOMapping=0")
            w()

        # Mapping record
        mappings = pdo["mappings"]
        w(f"[{map_base:04X}]")
        w(f"ParameterName={pdo_name.upper()} mapping parameter")
        w("ObjectType=0x9")
        w(";StorageLocation=PERSIST_COMM")
        w(f"SubNumber=0x{len(mappings)+1:X}")
        w()
        w(f"[{map_base:04X}sub0]")
        w("ParameterName=Number of mapped objects")
        w("ObjectType=0x7")
        w(";StorageLocation=RAM")
        w("DataType=0x0005")
        w("AccessType=rw")
        w(f"DefaultValue={len(mappings)}")
        w("PDOMapping=0")
        w()
        for i, m in enumerate(mappings, 1):
            packed = (m["index"] << 16) | (m["sub"] << 8) | m["bits"]
            w(f"[{map_base:04X}sub{i}]")
            w(f"ParameterName=Application object {i}")
            w("ObjectType=0x7")
            w(";StorageLocation=RAM")
            w("DataType=0x0007")
            w("AccessType=rw")
            w(f"DefaultValue=0x{packed:08X}")
            w("PDOMapping=0")
            w()

    # Manufacturer objects
    mfr_indices = []
    for _, entry in all_entries(registry):
        mfr_indices.append(entry["index"])

    w("[ManufacturerObjects]")
    w(f"SupportedObjects={len(mfr_indices)}")
    for i, idx in enumerate(mfr_indices, 1):
        w(f"{i}=0x{idx:04X}")
    w()

    # Each manufacturer entry
    for mod_name, entry in all_entries(registry):
        idx = entry["index"]
        nsub = entry.get("sub_indices", 0)
        dt = EDS_TYPES.get(entry["type"], "0x0007")
        access = entry["access"]
        default = entry.get("default", 0)
        if isinstance(default, int):
            default = str(default)
        pdo_map = 1 if entry.get("pdo") else 0

        if nsub > 0:
            # Array
            w(f"[{idx:04X}]")
            w(f"ParameterName={entry['name']}")
            w("ObjectType=0x8")
            w(";StorageLocation=RAM")
            w(f"SubNumber=0x{nsub+1:X}")
            w()
            w(f"[{idx:04X}sub0]")
            w("ParameterName=Number of entries")
            w("ObjectType=0x7")
            w(";StorageLocation=RAM")
            w("DataType=0x0005")
            w("AccessType=ro")
            w(f"DefaultValue=0x{nsub:02X}")
            w("PDOMapping=0")
            w()
            for s in range(1, nsub + 1):
                w(f"[{idx:04X}sub{s}]")
                w(f"ParameterName={entry['name']} {s}")
                w("ObjectType=0x7")
                w(";StorageLocation=RAM")
                w(f"DataType={dt}")
                w(f"AccessType={access}")
                w(f"DefaultValue={default}")
                w(f"PDOMapping={pdo_map}")
                w()
        else:
            # Scalar
            w(f"[{idx:04X}]")
            w(f"ParameterName={entry['name']}")
            w("ObjectType=0x7")
            w(";StorageLocation=RAM")
            w(f"DataType={dt}")
            w(f"AccessType={access}")
            w(f"DefaultValue={default}")
            w(f"PDOMapping={pdo_map}")
            w()

    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    Path(out_path).write_text("\n".join(L))
    print(f"  ✓ EDS:           {out_path}  ({len(mfr_indices)} mfr objects, {len(L)} lines)")


# ============================================================================
# 2. C++ named constants header — UC2_OD_Indices.h
# ============================================================================

def generate_cpp_indices(registry, out_path):
    L = []
    w = lambda s="": L.append(s)

    w("// AUTO-GENERATED from uc2_canopen_registry.yaml — DO NOT EDIT")
    w("// Regenerate with: python tools/regenerate_all.py")
    w("//")
    w("// This file provides named constants for all UC2 CANopen OD entries.")
    w("// Use these instead of magic numbers like 0x6401:01.")
    w("//")
    w("// Example:")
    w("//   CANopenModule::writeSDO(nodeId,")
    w("//       UC2_OD::MOTOR_TARGET_POSITION, 1,  // index, sub")
    w("//       (uint8_t*)&pos, sizeof(int32_t));")
    w()
    w("#pragma once")
    w("#include <stdint.h>")
    w()
    w("namespace UC2_OD {")
    w()

    for mod_name, mod in registry["modules"].items():
        w(f"// ─── {mod_name.upper()} (base 0x{mod['base_index']:04X}) ───")
        if mod.get("description"):
            w(f"// {mod['description']}")
        w()
        for entry in mod["entries"]:
            const_name = entry["name"].upper()
            w(f"constexpr uint16_t {const_name:<40} = 0x{entry['index']:04X};")
        w()

    w("}  // namespace UC2_OD")
    w()
    w("// Node-ID assignments")
    w("namespace UC2_NODE {")
    for role, nid in registry["node_ids"].items():
        w(f"constexpr uint8_t {role.upper():<20} = {nid};")
    w("}  // namespace UC2_NODE")
    w()

    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    Path(out_path).write_text("\n".join(L))
    n = sum(1 for _ in all_entries(registry))
    print(f"  ✓ C++ indices:   {out_path}  ({n} constants)")


# ============================================================================
# 3. Python named constants — uc2_indices.py
# ============================================================================

def generate_python_indices(registry, out_path):
    L = []
    w = lambda s="": L.append(s)

    w('"""')
    w("AUTO-GENERATED from uc2_canopen_registry.yaml — DO NOT EDIT")
    w("Regenerate with: python tools/regenerate_all.py")
    w()
    w("Use these constants when writing CANopen scripts via python-canopen")
    w("or the Waveshare USB-CAN adapter.")
    w()
    w("Example:")
    w("    import canopen")
    w("    from uc2_indices import OD, NODE")
    w("    network = canopen.Network()")
    w("    network.connect(channel='can0', bustype='socketcan')")
    w("    node = network.add_node(NODE.MOTOR_X, 'openUC2_satellite.eds')")
    w("    node.sdo[OD.MOTOR_TARGET_POSITION][1].raw = 1000")
    w('"""')
    w()
    w("class OD:")
    w('    """CANopen Object Dictionary indices for openUC2 satellite nodes."""')
    for mod_name, mod in registry["modules"].items():
        w(f"    # {mod_name.upper()} — {mod.get('description', '')}")
        for entry in mod["entries"]:
            const_name = entry["name"].upper()
            w(f"    {const_name} = 0x{entry['index']:04X}")
        w()

    w("class NODE:")
    w('    """Node-ID assignments for the openUC2 CAN bus."""')
    for role, nid in registry["node_ids"].items():
        w(f"    {role.upper()} = {nid}")
    w()

    w("# Reverse lookup: index → human name")
    w("OD_NAMES = {")
    for _, entry in all_entries(registry):
        w(f"    0x{entry['index']:04X}: {entry['name']!r},")
    w("}")
    w()

    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    Path(out_path).write_text("\n".join(L))
    print(f"  ✓ Python indices: {out_path}")


# ============================================================================
# 4. Markdown documentation
# ============================================================================

def generate_markdown(registry, out_path):
    L = []
    w = lambda s="": L.append(s)

    w("# UC2 CANopen Parameter Reference")
    w()
    w("**AUTO-GENERATED** from `uc2_canopen_registry.yaml`. Do not edit manually.")
    w()
    w(f"Regenerate with `python tools/regenerate_all.py`.")
    w()
    w("## Overview")
    w()
    w(f"- Vendor: {registry['device']['vendor_name']}")
    w(f"- Product: {registry['device']['product_name']}")
    w(f"- Vendor ID: `0x{registry['device']['vendor_id']:08X}`")
    w(f"- Default baud rate: {registry['device']['default_baudrate']} kbit/s")
    w()
    w("## Node-ID assignments")
    w()
    w("| Role | Node-ID |")
    w("|------|---------|")
    for role, nid in registry["node_ids"].items():
        w(f"| {role} | {nid} |")
    w()

    w("## OD index map")
    w()
    for mod_name, mod in registry["modules"].items():
        w(f"### {mod_name.title()} — base `0x{mod['base_index']:04X}`")
        w()
        if mod.get("description"):
            w(mod["description"])
            w()
        if mod.get("json_endpoint"):
            w(f"*Maps to JSON endpoint(s):* `{mod['json_endpoint']}`")
            w()
        if mod.get("cpp_class"):
            w(f"*C++ class:* `{mod['cpp_class']}`")
            w()
        w("| Index | Sub | Name | Type | Access | PDO | Description |")
        w("|-------|-----|------|------|--------|-----|-------------|")
        for entry in mod["entries"]:
            sub = entry.get("sub_indices", 0)
            sub_str = f"1..{sub}" if sub > 0 else "0"
            pdo = entry.get("pdo", "—")
            if entry.get("sdo_only"):
                pdo = "SDO only"
            desc = entry.get("description", "").replace("\n", " ").strip()
            if len(desc) > 80:
                desc = desc[:77] + "..."
            w(f"| `0x{entry['index']:04X}` | {sub_str} | `{entry['name']}` | {entry['type']} | {entry['access']} | {pdo} | {desc} |")
        w()

    w("## PDO mapping summary")
    w()
    for pdo_name, pdo in registry["pdo_mapping"].items():
        w(f"### {pdo_name.upper()}")
        w()
        w(f"- COB-ID: `{pdo['cob_id']}`")
        w(f"- Direction: {pdo['direction']}")
        w(f"- Description: {pdo['description']}")
        if pdo.get("event_timer_ms"):
            w(f"- Event timer: {pdo['event_timer_ms']} ms")
        if pdo.get("inhibit_time_ms"):
            w(f"- Inhibit time: {pdo['inhibit_time_ms']} ms")
        w()
        w("| # | Index:Sub | Bits | Maps to |")
        w("|---|-----------|------|---------|")
        for i, m in enumerate(pdo["mappings"], 1):
            # Find the entry name
            target_name = "?"
            for _, entry in all_entries(registry):
                if entry["index"] == m["index"]:
                    target_name = entry["name"]
                    break
            w(f"| {i} | `0x{m['index']:04X}:{m['sub']:02X}` | {m['bits']} | `{target_name}` |")
        w()

    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    Path(out_path).write_text("\n".join(L))
    print(f"  ✓ Markdown:      {out_path}")


# ============================================================================
# 5. CANopenNode v4 OD.h (skeleton — full table generation is large)
# ============================================================================

def generate_od_h(registry, out_path):
    """
    Generate a CANopenNode v4 OD.h header. This is a SIMPLIFIED version —
    for full production use, run the generated EDS through CANopenEditor's
    exporter, OR extend this generator. The CANopenNode OD format is well
    documented in libedssharp/EDSSharp.
    """
    L = []
    w = lambda s="": L.append(s)

    w("// AUTO-GENERATED from uc2_canopen_registry.yaml")
    w("// Regenerate with: python tools/regenerate_all.py")
    w("//")
    w("// This file is the CANopenNode v4 Object Dictionary header.")
    w("// It declares OD_RAM (the runtime data) and OD (the descriptor table).")
    w()
    w("#ifndef OD_H")
    w("#define OD_H")
    w()
    w('#include "301/CO_ODinterface.h"')
    w()
    w("#define OD_CNT_NMT                 1")
    w("#define OD_CNT_EM                  1")
    w("#define OD_CNT_SYNC                1")
    w("#define OD_CNT_HB_PROD             1")
    w("#define OD_CNT_HB_CONS             1")
    w("#define OD_CNT_RPDO                4")
    w("#define OD_CNT_TPDO                4")
    w()

    # The OD_RAM struct holds the runtime data
    w("// Runtime variables for OD entries")
    w("typedef struct {")

    # CiA 301 standard entries
    w("    // --- CiA 301 communication area ---")
    w("    uint8_t  x1001_errorRegister;")
    w("    uint16_t x1017_producerHeartbeatTime;")
    w()

    # Manufacturer entries
    w("    // --- Manufacturer-specific area ---")
    for mod_name, mod in registry["modules"].items():
        w(f"    // {mod_name}")
        for entry in mod["entries"]:
            ctype = C_TYPES[entry["type"]]
            name = f"x{entry['index']:04X}_{entry['name']}"
            sub = entry.get("sub_indices", 0)
            if entry["type"] == "STRING":
                w(f"    {ctype} {name}[{entry.get('max_length', 32)}];")
            elif entry["type"] == "DOMAIN":
                w(f"    {ctype}* {name};  // DOMAIN — pointer set at runtime")
            elif sub > 0:
                w(f"    {ctype} {name}[{sub}];")
            else:
                w(f"    {ctype} {name};")
        w()

    w("} OD_RAM_t;")
    w()
    w("extern OD_RAM_t OD_RAM;")
    w("extern OD_t *OD;")
    w()
    w("// PDO mapping helper macros (for syncRpdoToModules / syncModulesToTpdo)")
    for mod_name, mod in registry["modules"].items():
        for entry in mod["entries"]:
            macro = f"OD_{entry['name'].upper()}"
            field = f"OD_RAM.x{entry['index']:04X}_{entry['name']}"
            w(f"#define {macro:<48} {field}")
    w()
    w("#endif // OD_H")

    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    Path(out_path).write_text("\n".join(L))
    print(f"  ✓ OD.h:          {out_path}")


def generate_od_c(registry, out_path):
    """
    Generate a STUB OD.c. The full table generation requires reproducing
    the CANopenNode descriptor format. For production, prefer running the
    EDS through CANopenEditor's exporter and storing the result here.
    """
    L = []
    w = lambda s="": L.append(s)

    w("// AUTO-GENERATED from uc2_canopen_registry.yaml")
    w("// This is a STUB. For production use:")
    w("//   1. Run: python tools/regenerate_all.py")
    w("//   2. Open DOCUMENTATION/openUC2_satellite.eds in CANopenEditor")
    w("//   3. File → Export → CANopenNode v4 (OD.c only)")
    w("//   4. Replace this file with the exported one")
    w("//")
    w("// Long-term: extend tools/regenerate_all.py to emit a complete OD.c table.")
    w()
    w('#include "OD.h"')
    w()
    w("OD_RAM_t OD_RAM = { 0 };")
    w()

    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    Path(out_path).write_text("\n".join(L))
    print(f"  ✓ OD.c (stub):   {out_path}")


# ============================================================================
# Main
# ============================================================================

def main():
    if len(sys.argv) > 1:
        registry_path = sys.argv[1]
    else:
        # Default: relative to this script
        script_dir = Path(__file__).parent
        registry_path = script_dir / "uc2_canopen_registry.yaml"

    print(f"Reading registry: {registry_path}")
    registry = load_registry(registry_path)

    n_entries = sum(1 for _ in all_entries(registry))
    n_modules = len(registry["modules"])
    print(f"  {n_modules} modules, {n_entries} OD entries")
    print()
    print("Generating artifacts:")

    base = Path(script_dir).parent.parent

    generate_eds(registry,            base / "DOCUMENTATION/openUC2_satellite.eds")
    generate_cpp_indices(registry,    base / "main/src/canopen/UC2_OD_Indices.h")
    generate_python_indices(registry, base / "PYTHON/uc2_canopen/uc2_indices.py")
    generate_markdown(registry,       base / "DOCUMENTATION/UC2_CANopen_Parameters.md")
    generate_od_h(registry,           base / "lib/uc2_od/OD.h")
    generate_od_c(registry,           base / "lib/uc2_od/OD.c")

    print()
    print("Done. Commit all generated files together with the registry.")


if __name__ == "__main__":
    main()
