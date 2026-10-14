# T2.9 regression: xiao_odmr (IDF 6.1, esp32s3).
# Largest S3 app — motor + laser + LED + digIn + message + heat.
. "M:\.espressif\v6.1\esp-idf\export.ps1" | Out-Null
cd M:\repos\uc2-esp32\apps\xiao_odmr
idf.py build 2>&1 | Tee-Object -FilePath M:\repos\uc2-esp32\xiao_odmr_build.log | Select-Object -Last 60
"EXIT: $LASTEXITCODE"
