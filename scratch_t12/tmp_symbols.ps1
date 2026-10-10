$dirs = @(
 'C:\Users\troop\.platformio\packages\framework-espidf@4.50505.0\components\usb\include\usb',
 'C:\Users\troop\.platformio\packages\framework-espidf@4.50505.0\components\hid\include',
 'C:\Users\troop\.platformio\packages\framework-espidf@4.50505.0\components\esp_hid\include',
 'C:\Users\troop\.platformio\packages\framework-espidf@3.40407.240606\components\usb\include\usb'
)
foreach ($d in $dirs) {
  if (-not (Test-Path $d)) { "MISSING DIR: $d"; continue }
  $all = (Get-ChildItem $d -Filter *.h -Recurse | ForEach-Object { Get-Content $_.FullName -Raw }) -join "`n"
  "## $d"
  foreach ($n in @('hid_descriptor_t','HID_CLASS_DESCRIPTOR_TYPE_HID','HID_CLASS_DESCRIPTOR_TYPE_REPORT','usb_parse_interface_descriptor','usb_host_device_info','USB_SETUP_PACKET_SIZE')) {
    $hit = if ($all -match [regex]::Escape($n)) { 'Y' } else { 'n' }
    "   $hit  $n"
  }
}
