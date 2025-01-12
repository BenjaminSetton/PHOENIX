@echo off

call config.bat

:: Build GLFW
echo Building GLFW dependency...
set GLFW_BUILD_DIR=%WORKSPACE_DIR%PHOENIX\out\glfw
set GLFW_BUILD_DIR_DEB="%GLFW_BUILD_DIR%\debug"
set GLFW_BUILD_DIR_REL="%GLFW_BUILD_DIR%\release"

::echo workspace_dir - "%WORKSPACE_DIR%"
::echo build dir - "%GLFW_BUILD_DIR%"
::echo build_dir_db - "%GLFW_BUILD_DIR_DEB%"
::echo build_dir_rel - "%GLFW_BUILD_DIR_REL%"

cd %WORKSPACE_DIR%\PHOENIX\vendor\glfw
if not exist "%GLFW_BUILD_DIR%" mkdir "%GLFW_BUILD_DIR%"
cmake -S . -B build
cd build

echo Started building GLFW debug...
cmake --build . --config Debug
echo Finished building GLFW debug!

echo Started building GLFW release...
cmake --build . --config Release
echo Finished building GLFW release!

echo Finished building GLFW!