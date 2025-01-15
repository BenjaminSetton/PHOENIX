@echo off

call config.bat

:: Build GLFW
echo [PHOENIX] Building GLFW dependency...
::set GLFW_BUILD_DIR=%WORKSPACE_DIR%PHOENIX\out\glfw

::echo workspace_dir - "%WORKSPACE_DIR%"
::echo build dir - "%GLFW_BUILD_DIR%"
::echo build_dir_db - "%GLFW_BUILD_DIR_DEB%"
::echo build_dir_rel - "%GLFW_BUILD_DIR_REL%"

cd %WORKSPACE_DIR%\PHOENIX\vendor\glfw
::if not exist "%GLFW_BUILD_DIR%" mkdir "%GLFW_BUILD_DIR%"
cmake -S . -B build
cd build

echo [PHOENIX] Started building GLFW debug...
cmake --build . --config Debug
echo [PHOENIX] Finished building GLFW debug!

echo [PHOENIX] Started building GLFW release...
cmake --build . --config Release
echo [PHOENIX] Finished building GLFW release!

echo [PHOENIX] Finished building GLFW!

:: Build glslc
echo [PHOENIX] Building glslc dependency...

:: TODO

echo [PHOENIX] Finished building glslc dependency!

echo [PHOENIX] Finished building dependencies!
pause