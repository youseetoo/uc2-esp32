# T4.1: canopen_master — main build check after the C-port batch.
. "C:\esp\v6.1\esp-idf\export.ps1" | Out-Null
cd M:\repos\uc2-esp32\apps\canopen_master
idf.py build 2>&1 | Tee-Object -FilePath M:\repos\uc2-esp32\canopen_master_build.log | Select-Object -Last 70
"EXIT: $LASTEXITCODE"
