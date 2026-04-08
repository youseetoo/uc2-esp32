# UC2 CANopen Tooling

This directory contains the generator scripts that keep all CANopen artifacts
in sync from a single YAML registry.

```
tools/canopen/
├── uc2_canopen_registry.yaml   ← SINGLE SOURCE OF TRUTH
└── regenerate_all.py           ← generator
```

---

## Registry is the source of truth

`uc2_canopen_registry.yaml` defines every Object Dictionary entry that a UC2
satellite node can expose on CAN.  It drives the generation of:

| Generated file | Purpose |
|---|---|
| `DOCUMENTATION/openUC2_satellite.eds` | CiA 306 device description (open in CANopenEditor for visual review) |
| `DOCUMENTATION/UC2_CANopen_Parameters.md` | Human-readable parameter reference |
| `main/src/canopen/UC2_OD_Indices.h` | C++ named constants (`UC2_OD::MOTOR_TARGET_POSITION`, …) |
| `PYTHON/uc2_canopen/uc2_indices.py` | Python named constants for automation scripts |
| `lib/uc2_od/OD.h` | CANopenNode v4 OD header (skeleton — see note below) |
| `lib/uc2_od/OD.c` | CANopenNode v4 OD source (stub — see note below) |

Never edit those generated files directly. Edits will be overwritten on the
next `regenerate_all.py` run and the CI check will catch any drift.

---

## Workflow for adding a new parameter

1. Open `tools/canopen/uc2_canopen_registry.yaml`.
2. Locate (or create) the correct module block, e.g. `motor:`.
3. Add a new entry following the existing pattern:

   ```yaml
   - name: motor_velocity_limit
     index: 0x2007
     type: U32
     access: rw
     default: 10000
     pdo: false
     description: "Maximum velocity in steps/s"
   ```

4. Run the generator:

   ```bash
   python tools/canopen/regenerate_all.py
   # or
   make canopen-regen
   ```

5. Commit **both** the YAML change and all generated files in one commit:

   ```bash
   git add tools/canopen/uc2_canopen_registry.yaml \
           DOCUMENTATION/openUC2_satellite.eds \
           DOCUMENTATION/UC2_CANopen_Parameters.md \
           main/src/canopen/UC2_OD_Indices.h \
           PYTHON/uc2_canopen/uc2_indices.py \
           lib/uc2_od/OD.h lib/uc2_od/OD.c
   git commit -m "canopen: add motor_velocity_limit parameter"
   ```

The CI workflow (`.github/workflows/canopen-registry-check.yml`) verifies that
generated files are never ahead of or behind the registry.

---

## How to add a new module type

Module types group logically related OD entries under a shared base index.

1. Pick an unused base index from the index map in the registry (see the
   `# OD INDEX MAP` comment block near the top of the YAML).
2. Add a new top-level key under `modules:`:

   ```yaml
   modules:
     my_new_module:
       base_index: 0x2F00
       description: "My new hardware module"
       json_endpoint: "mymodule"
       cpp_class: "MyNewModule"
       entries:
         - name: my_param
           index: 0x2F00
           type: U16
           access: rw
           default: 0
   ```

3. Run `python tools/canopen/regenerate_all.py` and commit.

---

## Role of CANopenEditor

`DOCUMENTATION/openUC2_satellite.eds` is the generated device description in
CiA 306 INI format.  CANopenEditor is **not** the source of truth — but it is
useful for:

- **Visual validation** — inspect PDO mappings, sub-indices, and data types in
  a GUI before flashing.
- **Round-trip editing** (optional) — if you prefer to design entries visually,
  export back to EDS, then reconcile changes into the YAML manually.  Running
  the generator afterwards must produce an identical EDS.
- **Interoperability checks** — verify the EDS parses without errors in
  third-party tools (e.g. PEAK PCAN-Explorer, Wireshark's CANopen dissector).

The generator (`regenerate_all.py`) mirrors what CANopenEditor would emit for
the same logical structure, so loading the generated EDS in CANopenEditor should
open cleanly with no parse errors or missing mandatory objects.

> **Note on OD.h / OD.c:**  The generator produces a skeleton OD.h with inline
> `OD_RAM_t` struct members and macro aliases, plus a minimal stub OD.c.  For a
> full CANopenNode v4 descriptor table (needed when actually running CANopenNode
> on the slave), export from CANopenEditor (File → Export → CANopenNode v4) and
> replace `lib/uc2_od/OD.c` with the result.  This is planned for PR-7.

---

## Running the generator

```bash
# From the repo root:
python tools/canopen/regenerate_all.py

# Or via make:
make canopen-regen

# Or via PlatformIO (after wiring extra_scripts in platformio.ini):
~/.platformio/penv/bin/pio run -t canopen-regen
```
