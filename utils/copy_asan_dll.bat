@echo off
setlocal

:: Copies the MSVC ASAN runtime DLL into a single destination directory.
:: Usage: copy_asan_dll.bat "<destination_dir>"
:: Intended to be run as a Sanitizer-config post-build step per sample.

set "DEST=%~1"
set "DLL=clang_rt.asan_dynamic-x86_64.dll"

if "%DEST%"=="" ( echo [ASAN] ERROR: no destination dir given & exit /b 1 )

:: Copy only if missing
if exist "%DEST%\%DLL%" exit /b 0

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" ( echo [ASAN] ERROR: vswhere.exe not found & exit /b 1 )

for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -property installationPath`) do set "VS_INSTALL=%%I"

set "ASAN_DLL="
for /f "usebackq delims=" %%F in (`dir /s /b "%VS_INSTALL%\VC\Tools\MSVC\%DLL%" 2^>nul`) do if not defined ASAN_DLL set "ASAN_DLL=%%F"
if not defined ASAN_DLL ( echo [ASAN] ERROR: %DLL% not found under Visual Studio install & exit /b 1 )

copy /Y "%ASAN_DLL%" "%DEST%\%DLL%" >nul
echo [ASAN] Copied %DLL% to %DEST%
exit /b 0
