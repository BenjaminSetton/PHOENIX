@echo off

call config.bat

cd %WORKSPACE_DIR%

:: Build GLFW
echo [PHOENIX] Building GLFW dependency...

set GLFW_SRC=".\PHOENIX\vendor\glfw"
set GLFW_OUT="%OUTPUT_DIR%glfw"

cmake -S %GLFW_SRC% -B %GLFW_OUT% -D GLFW_BUILD_EXAMPLES=OFF -D GLFW_BUILD_TESTS=OFF -D GLFW_BUILD_DOCS=OFF

::-----------------------------------------------------------
echo [PHOENIX] Started building GLFW debug...
cmake --build %GLFW_OUT% --config Debug
echo [PHOENIX] Finished building GLFW debug!

echo [PHOENIX] Started building GLFW release...
cmake --build %GLFW_OUT% --config Release
echo [PHOENIX] Finished building GLFW release!
::-----------------------------------------------------------

echo [PHOENIX] Finished building GLFW!

:: Build glslang
echo [PHOENIX] Building glslang dependency...

::-----------------------------------------------------------
echo [PHOENIX] Pulling glslang dependencies...

set GLSLANG_SRC=".\PHOENIX\vendor\glslang"
set GLSLANG_OUT="%OUTPUT_DIR%glslang"

:: Push to glslang directory because python script looks for other files using relative paths from the project root
pushd "PHOENIX/vendor/glslang"
py ./update_glslang_sources.py
popd

echo [PHOENIX] Finished pulling glslang dependencies!
::-----------------------------------------------------------

cmake -S %GLSLANG_SRC% -B %GLSLANG_OUT% -D GLSLANG_TESTS=OFF

::-----------------------------------------------------------
echo [PHOENIX] Started building glslang debug...
cmake --build %GLSLANG_OUT% --config Debug
echo [PHOENIX] Finished building glslang debug!

echo [PHOENIX] Started building glslang release...
cmake --build %GLSLANG_OUT% --config Release
echo [PHOENIX] Finished building glslang release!
::-----------------------------------------------------------

echo [PHOENIX] Finished building glslang dependency!

echo [PHOENIX] Finished building dependencies!
pause