$d = 0
while (-not (Test-Path M:\) -and $d -lt 15) { Start-Sleep 2; $d++ }
if (-not (Test-Path M:\)) { Write-Host "M: drive not back"; exit 2 }
$bin = "M:\repos\uc2-esp32\scratch_t12\build\scratch_t12.bin"
$prev = (Get-Item $bin).LastWriteTime
& M:\repos\uc2-esp32\build_scratch_t12.ps1 *> M:\repos\uc2-esp32\scratch_t12\build_log_canopen.txt
$b = Get-Item $bin
Write-Host ("BIN " + $b.Length + "B at " + $b.LastWriteTime + " prev=" + $prev)
if ($b.LastWriteTime -le $prev) { Write-Host "BIN NOT REFRESHED, tail of build_log_canopen.txt:"; Get-Content M:\repos\uc2-esp32\scratch_t12\build_log_canopen.txt -Tail 40 } else { Write-Host "BIN FRESH" }
