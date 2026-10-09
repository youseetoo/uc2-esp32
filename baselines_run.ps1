$ErrorActionPreference = "Continue"
$pio  = "C:\Users\troop\.platformio\penv\Scripts\pio.exe"
$dir  = "M:\repos\uc2-esp32"
$logs = Join-Path $env:TEMP "uc2_baselines"
New-Item -ItemType Directory -Force -Path $logs | Out-Null
Set-Location $dir
$envs = @(
  "UC2_2","UC2_3","UC2_4","UC2_WEMOS",
  "seeed_xiao_esp32s3","seeed_xiao_esp32s3_ledservo","seeed_xiao_esp32s3_ledring",
  "UC2_canopen_slave_motor_release","UC2_canopen_slave_accelmotor_release",
  "UC2_canopen_slave_galvo_release","UC2_canopen_slave_laser_release",
  "UC2_canopen_slave_led_release","UC2_canopen_slave_gpio_release",
  "UC2_canopen_bridge_ps4_usbhost_release","UC2_canopen_master_release",
  "UC2_canopen_standalone_v4_release","waveshare_esp32s3_ledarray")
$failed = @()
foreach ($e in $envs) {
  $log = Join-Path $logs "$e.log"
  if (Test-Path $log) { Write-Host "SKIP $e (log exists)"; continue }
  Write-Host "BUILD $e ..."
  # Full build (not -t size): -t size leaves no firmware.bin; size target output
  # (text/data/bss) is still printed at the end of a full build.
  & $pio run -e $e 2>&1 | Out-File -Encoding utf8 $log
  $code = $LASTEXITCODE
  Write-Host "DONE $e (exit $code)"
  if ($code -ne 0) { $failed += $e }
}
Write-Host "FAILED: $($failed -join ', ')"
