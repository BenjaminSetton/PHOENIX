@echo off

:: Generate a Visual Studio solution
:: Visual Studio 2019: "vs2019"
:: Visual Studio 2022: "vs2022"
set GENERATOR="vs2022"
set BUILD_DIR="build"

echo Building PHOENIX for %GENERATOR%...

call "../PHOENIX/vendor/premake/premake5.exe" %GENERATOR%

echo Finished building PHOENIX in %BUILD_DIR% folder!

pause