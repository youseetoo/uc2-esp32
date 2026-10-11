$paths = @(
  @{ n='usb_wrap_hal.h';   a='M:\Espressif\v5.5.1\esp-idf\components\hal\include\hal';           b='M:/.espressif/v6.1\esp-idf\components\esp_hal_usb\include\hal' },
  @{ n='usb_wrap_types.h'; a='M:\Espressif\v5.5.1\esp-idf\components\hal\include\hal';           b='M:/.espressif/v6.1\esp-idf\components\esp_hal_usb\include\hal' },
  @{ n='usb_dwc_hal.h';    a='M:\Espressif\v5.5.1\esp-idf\components\hal\include\hal';           b='M:/.espressif/v6.1\esp-idf\components\esp_hal_usb\include\hal' },
  @{ n='usb_dwc_types.h';  a='M:\Espressif\v5.5.1\esp-idf\components\hal\include\hal';           b='M:/.espressif/v6.1\esp-idf\components\esp_hal_usb\include\hal' }
)
foreach ($p in $paths) {
  $a = Get-Content (Join-Path $p.a $p.n) -ErrorAction SilentlyContinue
  $b = Get-Content (Join-Path $p.b $p.n) -ErrorAction SilentlyContinue
  if ($null -eq $a -or $null -eq $b) {
    Write-Output ("{0} : MISSING (5.5={1}, 6.1={2})" -f $p.n, ($null -ne $a), ($null -ne $b))
    continue
  }
  $d = (Compare-Object $a $b).Count
  Write-Output ("{0} : 5.5={1} lines, 6.1={2} lines, diff={3}" -f $p.n, $a.Count, $b.Count, $d)
}
