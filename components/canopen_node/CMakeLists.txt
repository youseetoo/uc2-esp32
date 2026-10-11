# CANopenNode (C). The app's object dictionary is supplied by the uc2_od
# component, so this component's own OD.c is excluded (mirrors the
# library.json srcFilter "+<*> -<OD.c>"). Needs the TWAI driver.
file(GLOB_RECURSE _co_sources "*.c")
list(FILTER _co_sources EXCLUDE REGEX ".*/OD\\.c$")
idf_component_register(SRCS ${_co_sources}
                       INCLUDE_DIRS "." "301" "303" "304" "305" "309" "extra" "storage"
                        REQUIRES driver log)

# The legacy driver/twai.h (used by canopen_node) emits a #warning under IDF 6.1;
# suppress it so -Werror=cpp does not fail the build.
# -DOD_CNT_TIME=0: uc2_od's OD.h defines OD_ENTRY_H1012 (the UC2 OD really
#   contains 0x1012 at slot 8) but not OD_CNT_TIME, so CANopen.c's
#   `#if !defined OD_CNT_TIME` fallback would re-define OD_ENTRY_H1012 — a
#   benign warning under PIO, a hard error under IDF's -Werror (GCC's C
#   front end has no -Wredefinition switch to suppress it). Pinning
#   OD_CNT_TIME=0 mirrors the PIO build exactly (TIME object disabled).
# -Wno-alloc-size: CO_alloc_break_on_fail(co->TIME, CO_GET_CNT(TIME), ...)
#   expands to calloc(0, 44) in a dead branch (CO_GET_CNT(TIME) == 0), same
#   warning as under PIO.
target_compile_definitions(__idf_canopen_node PRIVATE OD_CNT_TIME=0)
target_compile_options(__idf_canopen_node PRIVATE -Wno-cpp -Wno-alloc-size)
