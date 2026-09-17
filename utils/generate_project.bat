@echo off

:: Configurable settings
set "ARCHITECTURE=x64"
set "GENERATOR=Visual Studio 17 2022"

cmake -B "%~dp0..\build" -S "%~dp0.." -G "%GENERATOR%" -A %ARCHITECTURE%
