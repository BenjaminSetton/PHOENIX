@echo off

call config.bat

:: Build GLFW
echo [PHOENIX] Building GLFW dependency...

cd %WORKSPACE_DIR%\PHOENIX\vendor\glfw
cmake -S . -B build
cd build

::-----------------------------------------------------------
echo [PHOENIX] Started building GLFW debug...
cmake --build . --config Debug
echo [PHOENIX] Finished building GLFW debug!

echo [PHOENIX] Started building GLFW release...
cmake --build . --config Release
echo [PHOENIX] Finished building GLFW release!
::-----------------------------------------------------------

echo [PHOENIX] Finished building GLFW!

:: Build glslc
echo [PHOENIX] Building glslc dependency...

::-----------------------------------------------------------
echo [PHOENIX] Pulling glslc dependencies...

cd %WORKSPACE_DIR%\PHOENIX\vendor\glslang
py ./update_glslang_sources.py

echo [PHOENIX] Finished pulling glslc dependencies!
::-----------------------------------------------------------

cmake -S . -B build -DGLSLANG_TESTS_DEFAULT=OFF
cd build

::-----------------------------------------------------------
echo [PHOENIX] Started building glslang debug...
cmake --build . --config Debug
echo [PHOENIX] Finished building glslang debug!

echo [PHOENIX] Started building glslang release...
cmake --build . --config Release
echo [PHOENIX] Finished building glslang release!
::-----------------------------------------------------------

echo [PHOENIX] Finished building glslc dependency!

echo [PHOENIX] Finished building dependencies!
pause