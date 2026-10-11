# T1.3 scratch app build (IDF 6.1, esp32s3).
# Rebuilds scratch_t12/build via idf.py. Usage: .\build_scratch_t12.ps1
. "M:\.espressif\v6.1\esp-idf\export.ps1" | Out-Null
cd M:\repos\uc2-esp32\scratch_t12
idf.py build
