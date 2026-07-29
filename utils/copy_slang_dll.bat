@echo off
setlocal

:: Copies the Slang compiler DLL into a destination directory.
:: Usage: copy_slang_dll.bat "<destination_dir>" "<config>" "<source_dir>"
:: Intended to be run as a post-build step per sample.

set "DEST=%~1"
set "CONFIG=%~2"
set "SRC_DIR=%~3"

if "%DEST%"=="" ( echo [SLANG] ERROR: no destination dir given & exit /b 1 )
if "%CONFIG%"=="" ( echo [SLANG] ERROR: no config given & exit /b 1 )
if "%SRC_DIR%"=="" ( echo [SLANG] ERROR: no source dir given & exit /b 1 )

:: Determine DLL name based on config (Debug/Sanitizer use 'd' postfix)
set "SUFFIX="
if /I "%CONFIG%"=="Debug" set "SUFFIX=d"
if /I "%CONFIG%"=="Sanitizer" set "SUFFIX=d"
set "DLL=slang-compiler%SUFFIX%.dll"

set "SRC=%SRC_DIR%\%DLL%"

if not exist "%SRC%" ( echo [SLANG] ERROR: %DLL% not found at %SRC% & exit /b 1 )

copy /Y "%SRC%" "%DEST%\%DLL%" >nul
echo [SLANG] Copied %DLL% to %DEST%

:: Copy slang-glslang and slang-glsl-module DLLs.
:: Slang loads these by hardcoded name (no debug postfix), so strip 'd'.
set "GLSLANG_SRC=%SRC_DIR%\slang-glslang%SUFFIX%.dll"
if exist "%GLSLANG_SRC%" (
    copy /Y "%GLSLANG_SRC%" "%DEST%\slang-glslang.dll" >nul
    echo [SLANG] Copied slang-glslang%SUFFIX%.dll to %DEST%
) else (
    echo [SLANG] WARNING: slang-glslang%SUFFIX%.dll not found at %GLSLANG_SRC%
)

set "GLSLMOD_SRC=%SRC_DIR%\slang-glsl-module%SUFFIX%.dll"
if exist "%GLSLMOD_SRC%" (
    copy /Y "%GLSLMOD_SRC%" "%DEST%\slang-glsl-module.dll" >nul
    echo [SLANG] Copied slang-glsl-module%SUFFIX%.dll to %DEST%
) else (
    echo [SLANG] WARNING: slang-glsl-module%SUFFIX%.dll not found at %GLSLMOD_SRC%
)

:: Delete stale GLSL module cache to force loading from DLL
if exist "%DEST%\slang-glsl-module.bin" del /F "%DEST%\slang-glsl-module.bin" >nul 2>&1
exit /b 0
