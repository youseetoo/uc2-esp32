# T0.5 host-native test runner — MSVC variant (run.sh is the bash twin).
#
# Builds ONE test binary from test/unity/test_*.cpp (13 files incl. test_all.cpp,
# 123 registered tests) + the production sources the suite exercises, using
# test/unity/stubs/ as the Arduino/ESP-IDF/FreeRTOS host layer.
#
# Usage (PowerShell):
#   powershell -File test/unity/run_tests.ps1            # build + run
#   powershell -File test/unity/run_tests.ps1 -BuildOnly
#   powershell -File test/unity/run_tests.ps1 -RunOnly
param([switch]$BuildOnly, [switch]$RunOnly)
$ErrorActionPreference = "Stop"

$scriptDir = (Resolve-Path (Split-Path -Parent $MyInvocation.MyCommand.Path)).Path
$repo = Split-Path -Parent $scriptDir   # <repo>/test
$repo = Split-Path -Parent $repo        # <repo>
Set-Location $repo

$buildDir = "build\host_test"
$objDir   = "$buildDir\obj"
$bin      = "$buildDir\unity_tests.exe"
New-Item -ItemType Directory -Force -Path $objDir | Out-Null
# Drop stale objects: the source set changes between builds (a .cpp can move
# between "$prodSrcs" and a test's #include), and leftover .obj files get pulled
# into the link by basename, causing LNK2005 duplicate symbols.
Get-ChildItem $objDir -Filter *.obj -ErrorAction SilentlyContinue | Remove-Item -Force

# ── Locate Visual Studio ─────────────────────────────────────────────────────
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere not found at $vswhere" }
$vsPath = & $vswhere -latest -products * -property installationPath
if (-not $vsPath) { throw "No Visual Studio installation found" }
$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
Write-Host "Using VS: $vsPath"

# ── Flags / sources (mirror run.sh) ──────────────────────────────────────────
$inc = ("/Itest/unity/stubs /Itest/unity /Itest/unity/unity " +
        "/Imain /Imain/config/UC2_3_default /Imain/cJSON " +
        "/Ilib/ESP32_CanOpenNode /Ilib/ESP32_CanOpenNode/src /Ilib/uc2_od")
# UNITY_INCLUDE_DOUBLE: this Unity build EXCLUDES double-precision asserts by
# default (unity_internals.h:304), so TEST_ASSERT_EQUAL_DOUBLE fails with
# "Unity Double Precision Disabled". It must be defined for BOTH the test
# .cpp TUs (which call the macro) and unity.c (the macro implementation) or the
# generated test doubles won't match the runtime.
$defs = "/DUNIT_TEST /DCORE_DEBUG_LEVEL=5 /DUNITY_INCLUDE_DOUBLE"

# All paths relative to the repo root (the batch cd's there).
$testSrcs  = @(Get-ChildItem "test\unity\test_*.cpp" | ForEach-Object { $_.FullName.Substring($repo.Length + 1) })
$testSrcs  += @("test\unity\app_main_stubs.cpp")
$testSrcs  += @(Get-ChildItem "test\unity\mocks\*.cpp" | ForEach-Object { $_.FullName.Substring($repo.Length + 1) })
$testSrcs  += @("test\unity\stubs\host_impl.cpp")

# Production sources referenced by the suite but NOT #included by any test
# file. (The Unity tests use the "include the .cpp under test" pattern, so the
# remaining controllers — SerialProtocol, NVSConfig, CANopenModule, RoutingTable,
# AxisController, LedController, LaserController, MotorJsonParser, DeviceRouter —
# are already compiled into their test_*.obj and must NOT be recompiled here or
# the link fails with LNK2005 duplicate symbols.)
$prodSrcs  = @(
    "main\src\serial\SerialProcess.cpp",
    "main\src\motor\StageScan.cpp",
    # Dependencies of the test-target .cpp files: the Unity tests #include the
    # file under test only, so the other production .cpp files it calls into
    # must be compiled here or their symbols stay unresolved at link.
    "main\src\axis\AxisCalibration.cpp",
    "main\src\axis\AxisWatchdog.cpp",
    "main\src\axis\EncoderBackend.cpp",
    "main\src\axis\EncoderMonitor.cpp",
    "main\src\axis\AxisNotify.cpp",
    "main\src\motor\FocusMotor.cpp",
    "main\src\qid\QidRegistry.cpp",
    "main\src\state\State.cpp",
    "main\src\buzzer\BuzzerController.cpp",
    "main\src\signal\SignalController.cpp"
)
$cSrcs = @(
    "main\cJSON\cJSON.c",
    "test\unity\unity\unity.c"
)

# ── Generate build batch ─────────────────────────────────────────────────────
$batLines = @()
$batLines += '@echo off'
$batLines += "call `"$vcvars`" || exit /b 1"
$batLines += "cd /d `"$repo`""

# cl /Fo"dir\" names each object after the source BASENAME (e.g. test_all.cpp ->
# test_all.obj), so the link line must reference those names — not the full path.
function Get-HostTestObjName([string]$f) {
    $leaf = Split-Path $f -Leaf
    return ([System.IO.Path]::GetFileNameWithoutExtension($leaf) + ".obj")
}
$cppObjs = @()
foreach ($f in ($testSrcs + $prodSrcs + $cSrcs)) {
    $cppObjs += "`"$objDir\$(Get-HostTestObjName $f)`""
}

# One combined cl invocation per language (parallel-ish is not available; fine).
# /c = compile only (NO link): without it cl links the files it just compiled
# into an exe (test_all.exe) and dies on the unresolved cJSON/Unity symbols
# before the dedicated `link` step below ever runs.
$batLines += ("cl /nologo /std:c++17 /W3 /Zi /Od /c $inc $defs /Fo`"$objDir\\`" " +
    (($testSrcs + $prodSrcs) -join " ") + " || exit /b 1")
$batLines += ("cl /nologo /std:c11 /W3 /Zi /Od /c $inc /DUNITY_INCLUDE_DOUBLE /Fo`"$objDir\\`" " +
    ($cSrcs -join " ") + " || exit /b 1")
$batLines += ("link /nologo /SUBSYSTEM:CONSOLE " + ($cppObjs -join " ") +
    " /OUT:`"$bin`" || exit /b 1")

$batPath = "$buildDir\_build.bat"
Set-Content -Path $batPath -Value $batLines -Encoding ascii

# ── Run ──────────────────────────────────────────────────────────────────────
if (-not $RunOnly) {
    Write-Host "▶ Building (batch: $batPath)"
    & cmd /c "`"$batPath`""
    if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)" }
}
if ($BuildOnly) { exit 0 }

Write-Host "▶ Running $bin"
& $bin
exit $LASTEXITCODE
