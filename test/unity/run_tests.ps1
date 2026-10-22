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
# T4: components moved from main/src into components/uc2_*. Keep the include
# dirs in sync with where headers actually live (relative to repo root).
$inc = ("/Itest/unity/stubs /Itest/unity /Itest/unity/unity " +
        "/Icomponents/cjson /Icomponents/uc2_app /Icomponents/uc2_axis/axis " +
        "/Icomponents/uc2_core/config /Icomponents/uc2_core/state " +
        "/Icomponents/uc2_core/qid /Icomponents/uc2_core/buzzer " +
        "/Icomponents/uc2_hal/include " +
        "/Icomponents/uc2_motor/motor /Icomponents/uc2_motor/tmc /Icomponents/uc2_motor/home " +
        "/Icomponents/uc2_hal " +
        "/Icomponents/uc2_bt " +
        "/Icomponents/uc2_io/laser /Icomponents/uc2_led/led " +
        "/Icomponents/uc2_io/digitalin /Icomponents/uc2_periph/scanner " +
        "/Icomponents/uc2_led/signal /Icomponents/uc2_core/message " +
        "/Icomponents/uc2_io/digitalout /Icomponents/uc2_io/encoder " +
        "/Icomponents/uc2_motor/pid " +
        "/Icomponents/uc2_periph/fan /Icomponents/uc2_periph/heat " +
        "/Icomponents/uc2_periph/i2c /Icomponents/uc2_periph/thermal " +
        "/Icomponents/uc2_periph/tmp102 " +
        "/Icomponents/uc2_serial/serial /Icomponents/uc2_canopen/canopen " +
        "/Icomponents/canopen_node " +
        "/Iapps/canopen_master/main " +
        "/Ilib/ESP32_CanOpenNode /Ilib/ESP32_CanOpenNode/src /Ilib/uc2_od")
# UNITY_INCLUDE_DOUBLE: this Unity build EXCLUDES double-precision asserts by
# default (unity_internals.h:304), so TEST_ASSERT_EQUAL_DOUBLE fails with
# "Unity Double Precision Disabled". It must be defined for BOTH the test
# .cpp TUs (which call the macro) and unity.c (the macro implementation) or the
# generated test doubles won't match the runtime.
$defs = "/DUNIT_TEST /DCORE_DEBUG_LEVEL=5 /DUNITY_INCLUDE_DOUBLE /DCAN_CONTROLLER_CANOPEN"

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
# T4: production sources now live under components/uc2_*. C sources compile
# with /std:c11 below (cl would otherwise treat .c as C++); no test #includes
# a .c file anymore, so there is no mixed-language TU.
$prodSrcs  = @(
    "components\uc2_serial\serial\SerialProcessIn.cpp",
    # uc2::Serial_ + the `Serial`/`Serial1` globals (uc2_serial.h);
    # SerialProcessIn.cpp binds to them.
    "components\uc2_hal\src\uc2_serial.cpp",
    # C-linkage serial shims (uc2_serial_write_bytes / uc2_serial_flush)
    # called by the C TU uc2_serial_out.c.
    "components\uc2_hal\src\uc2_serial_c.cpp",
    # file under test only, so the other production sources it calls into
    # must be compiled here or their symbols stay unresolved at link.
    "components\uc2_axis\axis\EncoderBackend.cpp",
    # C-linked Arduino-compat GPIO (uc2_gpio.h declares C linkage); the C TUs
    # (State.c, StageScan.c) call pinMode/digitalWrite/digitalRead and bind to
    # these — the Arduino.h versions in host_impl.cpp are C++ linkage.
    "components\uc2_hal\src\uc2_gpio.cpp",
    # Single definition of `const EspClass ESP` (uc2_esp.h; SerialProcessIn.cpp
    # and the test TUs call ESP.*).
    "components\uc2_hal\src\uc2_esp.cpp",
    # C-linkage Wire shims (wire_begin / wire_begin_transmission / wire_write /
    # wire_request_from / wire_read / wire_end_transmission) called by the C
    # TUs FanController.c and Tmp102Controller.c.
    "components\uc2_hal\src\uc2_wire.cpp",
    # C-linkage ADC shims (analogRead(int) / analogSetPinAttenuation /
    # analogReadMilliVolts) called by the C TUs PidController.c and
    # ThermalController.c. (Distinct from host_impl.cpp's C++-linkage
    # analogRead(uint8_t) — different mangled name, no duplicate.)
    "components\uc2_hal\src\uc2_adc.cpp",
    # C++ islands with extern "C" shims that the JSON dispatch calls (T4.x):
    # galvo_*, the OTA binary receive state machine, and the BT controller
    # methods SerialProcessIn.cpp references. (TMCController.cpp compiles in
    # the separate $tmcSrcs step below — it needs the tmcstepper shim
    # include order.)
    "components\uc2_periph\scanner\GalvoController.cpp",
    # GalvoController's static scanner_ member (ctor/dtor/init/setConfig/
    # start/stop/...) lives here — without it the link dies with 11
    # HighSpeedScannerCore::* LNK2019s from GalvoController.obj.
    "components\uc2_periph\scanner\HighSpeedScannerCore.cpp",
    # C-linkage signal_strip_* shims (create/begin/set_brightness/clear/show/
    # gamma8/color/set_pixel) called by the C TU SignalController.c.
    "components\uc2_led\signal\signal_neopixel_c.cpp",
    "components\uc2_canopen\canopen\OtaBinaryReceive.cpp",
    "components\uc2_bt\BtController.cpp"
)
$cSrcs = @(
    "components\uc2_axis\axis\AxisCalibration.c",
    "components\uc2_axis\axis\AxisWatchdog.c",
    "components\uc2_axis\axis\AxisNotify.c",
    "components\uc2_axis\axis\EncoderMonitor.c",
    # AxisController (T4.2: the C++ wrapper is gone — the C API IS the API).
    "components\uc2_axis\axis\AxisController.c",
    # C-port islands the JSON dispatch / device router call (T4.x):
    "components\uc2_app\pinconfig_defaults.c",
    "components\uc2_core\config\ConfigController.c",
    "components\uc2_core\config\NVSConfig.c",
    "components\uc2_core\message\MessageController.c",
    "components\uc2_io\digitalin\DigitalInController.c",
    "components\uc2_io\digitalout\DigitalOutController.c",
    "components\uc2_io\encoder\InterruptController.c",
    "components\uc2_io\laser\LaserController.c",
    "components\uc2_led\led\ws2812_rmt.c",
    "components\uc2_led\signal\SignalController.c",
    "components\uc2_motor\home\HomeMotor.c",
    "components\uc2_motor\motor\FocusScan.c",
    "components\uc2_motor\motor\MotorGamePad.c",
    "components\uc2_motor\motor\MotorJsonParser.c",
    "components\uc2_motor\motor\axis_motor_c.c",
    "components\uc2_motor\motor\focusmotor_qid_c.c",
    "components\uc2_motor\pid\PidController.c",
    "components\uc2_periph\fan\FanController.c",
    "components\uc2_periph\heat\HeatController.c",
    "components\uc2_periph\i2c\I2cBridge.c",
    "components\uc2_periph\scanner\dac_mcp4822.c",
    "components\uc2_periph\thermal\ThermalController.c",
    "components\uc2_periph\tmp102\Tmp102Controller.c",
    "components\uc2_serial\serial\uc2_serial_out.c",
    "components\uc2_motor\motor\FocusMotor.c",
    "components\uc2_motor\motor\StageScan.c",
    "components\uc2_core\qid\QidRegistry.c",
    "components\uc2_core\state\State.c",
    "components\uc2_core\buzzer\BuzzerController.c",
    # Pure-C object dictionary (OD_RAM / OD_STATUS_BITS / OD_extension_init ...);
    # OD_find itself comes from the mock (mocks/mock_canopen.cpp) — no duplicate.
    "components\uc2_od\OD.c",
    "components\cjson\cJSON.c",
    "test\unity\unity\unity.c"
)

# T4.1 TMC island: TMCController.cpp + the vendored teemuatlut/TMCStepper
# sources. These TUs must resolve <Arduino.h> / <SPI.h> to the tmcstepper
# component's own shims (Stream = uc2::Serial_ typedef) BEFORE
# test/unity/stubs, or the vendor Stream* parameters bind to the stub's
# Stream class and `&Serial1` (uc2::Serial_) no longer converts. Separate cl
# invocation with the shim dirs prepended; objects join the same link.
$tmcSrcs = @("components\uc2_motor\tmc\TMCController.cpp")
$tmcSrcs += @(Get-ChildItem "components\tmcstepper\source\*.cpp" |
    Where-Object { $_.Name -notlike "bcm2835_*" } |
    ForEach-Object { $_.FullName.Substring($repo.Length + 1) })

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
foreach ($f in ($testSrcs + $prodSrcs + $cSrcs + $tmcSrcs)) {
    $cppObjs += "`"$objDir\$(Get-HostTestObjName $f)`""
}

# One combined cl invocation per language (parallel-ish is not available; fine).
# /c = compile only (NO link): without it cl links the files it just compiled
# into an exe (test_all.exe) and dies on the unresolved cJSON/Unity symbols
# before the dedicated `link` step below ever runs.
# /D_USE_MATH_DEFINES: exposes M_PI etc. from <math.h> (HighSpeedScannerCore.cpp
# uses M_PI; MSVC hides the constants without this define).
$batLines += ("cl /nologo /std:c++20 /W3 /Zi /Od /D_USE_MATH_DEFINES /c $inc $defs /Fo`"$objDir\\`" " +
    (($testSrcs + $prodSrcs) -join " ") + " || exit /b 1")
# TMC island: tmcstepper shims (".", "include") first, then the usual dirs.
# /Zc:__cplusplus is MANDATORY here: the vendor umbrella (TMCStepper.h:9-27)
# only pulls in <Arduino.h> (the Stream=uc2::Serial_ shim) when
# `__cplusplus >= 201703L`, and MSVC reports 199714L without this flag even
# under /std:c++20 — without it the vendor headers see no Stream/uint16_t.
$tmcInc = "/Icomponents/tmcstepper /Icomponents/tmcstepper/include " + $inc
$batLines += ("cl /nologo /std:c++20 /Zc:__cplusplus /W3 /Zi /Od /c $tmcInc $defs /Fo`"$objDir\\`" " +
    ($tmcSrcs -join " ") + " || exit /b 1")
# /DSTAGE_SCAN=1: MotorJsonParser.c's stage-scan entry point
# (motorjson_parse_stage_scan, called by DeviceRouter::handleMotorAct) is
# #ifdef STAGE_SCAN — without the define the C island compiles it out and the
# link fails on the missing symbol.
$batLines += ("cl /nologo /std:c11 /W3 /Zi /Od /c $inc /DSTAGE_SCAN=1 /DUNITY_INCLUDE_DOUBLE /Fo`"$objDir\\`" " +
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
