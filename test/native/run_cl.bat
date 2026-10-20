@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d M:\repos\uc2-esp32
cl /nologo /W3 /EHsc /Fe:stage_scan_order.exe test\native\test_stage_scan_order.cpp
if errorlevel 1 exit /b 1
stage_scan_order.exe
