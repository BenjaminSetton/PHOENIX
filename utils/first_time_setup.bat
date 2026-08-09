@echo off

:: Configurable settings
set "CONFIG=All"
set "ARCHITECTURE=x64"
set "GENERATOR=Visual Studio 17 2022"

echo Running first time setup...

:: Dependencies
powershell -ExecutionPolicy Bypass -File "%~dp0build_lib_dependencies.ps1" -Config %CONFIG% -Architecture %ARCHITECTURE%
if %errorlevel% neq 0 exit /b %errorlevel%
powershell -ExecutionPolicy Bypass -File "%~dp0build_sample_dependencies.ps1" -Config %CONFIG% -Architecture %ARCHITECTURE%
if %errorlevel% neq 0 exit /b %errorlevel%

:: Generate VS solution
cmake -B "%~dp0..\build" -S "%~dp0.." -G "%GENERATOR%" -A %ARCHITECTURE%
if %errorlevel% neq 0 exit /b %errorlevel%

echo Finished first time setup!
