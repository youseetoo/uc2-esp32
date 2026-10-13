$rsp = Get-Content "build\CMakeFiles\xiao_ledservo.elf.rsp" -Raw
# Find the context around whole-archive
$idx = $rsp.IndexOf('whole-archive')
if ($idx -ge 0) {
    $start = [Math]::Max(0, $idx - 200)
    $end = [Math]::Min($rsp.Length, $idx + 200)
    Write-Output $rsp.Substring($start, $end - $start)
}
