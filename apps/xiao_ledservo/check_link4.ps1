$rsp = Get-Content "build\CMakeFiles\xiao_ledservo.elf.rsp" -Raw
$idx = $rsp.IndexOf('whole-archive')
if ($idx -ge 0) {
    $start = [Math]::Max(0, $idx - 100)
    $end = [Math]::Min($rsp.Length, $idx + 300)
    Write-Output "Context around whole-archive:"
    Write-Output $rsp.Substring($start, $end - $start)
    Write-Output "---"
} else {
    Write-Output "No whole-archive found in rsp"
}
# Check if the RMT lib path with full path is in the rsp
if ($rsp -match 'components/esp_driver_rmt') {
    Write-Output "FOUND components/esp_driver_rmt path in rsp"
} else {
    Write-Output "No full RMT path in rsp"
}
