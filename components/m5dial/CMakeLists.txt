# T5.4: M5Dial (M5Stack Dial board API) — PIO-only leaf, wrapped as an IDF
# component. Thin wrapper over M5Unified + M5GFX (the M5Dial device API) plus the
# vendored Encoder (PCNT-backed) and MFRC522 (NFC) utilities. Compiled only by apps
# that REQUIRES it (m5stack_dial); the M5DIAL app define gates its use in code.
set(srcs
    "src/M5Dial.cpp"
    "src/utility/Encoder.cpp"
    "src/utility/MFRC522.cpp")
idf_component_register(SRCS ${srcs}
    INCLUDE_DIRS "src"
    REQUIRES m5unified m5gfx uc2_hal)
# T5.4: vendored C++ (not -Werror-clean) — relax diagnostics for this component
# only (mirrors the PIO env's build_unflags = -Werror=all).
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error -Wno-unused-variable
    -Wno-format -Wno-missing-field-initializers -Wno-sign-compare
    -Wno-unused-parameter -Wno-type-limits)
