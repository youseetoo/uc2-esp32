# T3.3 regression: canopen_slave_laser (IDF 6.1, esp32s3) after uc2_bt/pscontroller move.
# Confirms the moved HidGamePad.h (now in components/uc2_bt) still resolves for uc2_io/laser.
. "M:\.espressif\v6.1\esp-idf\export.ps1" | Out-Null
cd M:\repos\uc2-esp32\apps\canopen_slave_laser
idf.py build 2>&1 | Tee-Object -FilePath M:\repos\uc2-esp32\laser_build_check.log | Select-Object -Last 45
"EXIT: $LASTEXITCODE"
