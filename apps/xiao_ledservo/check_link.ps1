$rsp = Get-Content "build\CMakeFiles\xiao_ledservo.elf.rsp" -Raw
$libs = [regex]::Matches($rsp, 'esp-idf/[^ ]+\.a')
Write-Output "Total libs: $($libs.Count)"
for ($i=0; $i -lt $libs.Count; $i++) {
    if ($libs[$i].Value -match 'esp_driver_rmt|neopixel|esp_hal_rmt|esp_hal_dma') {
        Write-Output ("{0}: {1}" -f $i, $libs[$i].Value)
    }
}
