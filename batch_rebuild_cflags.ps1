# Batch clean rebuild of the 17 apps whose CMakeLists gained C_FLAGS feature
# macros (T4.4 C-ification fix). Clean build/ each: stale caches referenced an
# old ESP-IDF path and would hide reconfigure errors.
. "C:\esp\v6.1\esp-idf\export.ps1" | Out-Null
$apps = @(
  "waveshare_d7","canopen_bridge_ps4_usbhost","uc2_2","uc2_3","uc2_4","uc2_wemos",
  "canopen_slave_accelmotor","canopen_slave_galvo","canopen_slave_laser",
  "canopen_slave_led","canopen_slave_motor","canopen_standalone_v4","m5stack_dial",
  "canopen_bridge_ptz","xiao_ledring","xiao_ledservo","canopen_slave_gpio")
$summary = "M:\repos\uc2-esp32\batch_cflags_summary.txt"
Remove-Item $summary -ErrorAction SilentlyContinue
foreach ($a in $apps) {
  $dir = "M:\repos\uc2-esp32\apps\$a"
  Remove-Item "$dir\build" -Recurse -Force -ErrorAction SilentlyContinue
  Push-Location $dir
  idf.py build 2>&1 | Tee-Object -FilePath "M:\repos\uc2-esp32\batch_cflags_$a.log" | Out-Null
  $code = $LASTEXITCODE
  Pop-Location
  if ($code -eq 0) {
    $bin = Get-ChildItem "$dir\build" -Filter *.bin -ErrorAction SilentlyContinue |
      Where-Object { $_.Name -notmatch "bootloader|partition" } |
      Sort-Object Length -Descending | Select-Object -First 1
    if ($bin) { Add-Content $summary "OK   $a  $($bin.Name)  $($bin.Length) bytes" }
    else { Add-Content $summary "OK   $a  (no app bin found)" }
  } else {
    Add-Content $summary "FAIL $a  exit $code"
  }
}
Add-Content $summary "DONE"
"DONE"
