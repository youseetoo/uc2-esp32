# T1.3 build check: IDF 6.1, scratch_t12 (esp32s3)
Set-Location M:\repos\uc2-esp32\scratch_t12
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
. M:\.espressif\v6.1\esp-idf\export.ps1 | Out-Null
idf.py build 2>&1 | Select-Object -Last 40
Write-Output "IDF_BUILD_EXIT=$LASTEXITCODE"
