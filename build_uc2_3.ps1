# T3.4: uc2_3 (IDF 6.1, classic esp32, Bluedroid dual-mode BT) — first BT board port.
. "M:\.espressif\v6.1\esp-idf\export.ps1" | Out-Null
cd M:\repos\uc2-esp32\apps\uc2_3
idf.py build 2>&1 | Tee-Object -FilePath M:\repos\uc2-esp32\uc2_3_build.log | Select-Object -Last 70
"EXIT: $LASTEXITCODE"
