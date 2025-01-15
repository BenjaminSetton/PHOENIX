@echo off

call config.bat

echo Building PHOENIX for %GENERATOR%...

cd %WORKSPACE_DIR%
call "./PHOENIX/vendor/premake/premake5.exe" %GENERATOR%

echo Finished building PHOENIX!

pause