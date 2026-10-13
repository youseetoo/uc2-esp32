$rsp = Get-Content "build\CMakeFiles\xiao_ledservo.elf.rsp" -Raw
$libs = [regex]::Matches($rsp, 'esp-idf/[^ ]+\.a')
for ($i=0; $i -lt $libs.Count; $i++) {
    if ($libs[$i].Value -match 'esp_driver_rmt') {
        Write-Output ("Position {0}: {1}" -f $i, $libs[$i].Value)
    }
}
# Also check if there's a --start-group or --end-group
if ($rsp -match 'start-group') { Write-Output "FOUND start-group" } else { Write-Output "No start-group" }
if ($rsp -match 'whole-archive') { Write-Output "FOUND whole-archive" } else { Write-Output "No whole-archive" }
