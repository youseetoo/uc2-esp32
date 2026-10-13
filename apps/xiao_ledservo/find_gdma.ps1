$nm = "C:\Users\troop\.espressif\tools\xtensa-esp-elf\esp-15.2.0_20251204\xtensa-esp-elf\bin\xtensa-esp-elf-nm.exe"
Get-ChildItem "build\esp-idf" -Recurse -Filter "*.a" | ForEach-Object {
    $r = & $nm $_.FullName 2>&1 | Select-String "gdma_link_concat" | Select-Object -First 1
    if ($r) { Write-Output ($_.Name + ": " + $r) }
}
