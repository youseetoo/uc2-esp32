$m = Select-String -Path 'M:\repos\uc2-esp32\components\esp_usb_host_hid\hid_host.c','M:\repos\uc2-esp32\components\esp_usb_host_hid\hid_host.h' -Pattern 'USB_[A-Z0-9_]+|usb_[a-z0-9_]+(?= ?[(,;])' -AllMatches
$m.Matches.Value | Sort-Object -Unique
