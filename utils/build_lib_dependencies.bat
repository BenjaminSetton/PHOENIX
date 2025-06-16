@echo off

call config.bat

cd %WORKSPACE_DIR%

set LOG_CHANNEL=PHOENIX

:: Build GLFW
::-----------------------------------------------------------
echo [%LOG_CHANNEL%] Building GLFW dependency...

set GLFW_SRC=".\PHOENIX\vendor\glfw"
set GLFW_OUT="%LIB_OUTPUT_DIR%glfw"

:: Build GLFW project
cmake -S %GLFW_SRC% -B %GLFW_OUT% -D GLFW_BUILD_EXAMPLES=OFF -D GLFW_BUILD_TESTS=OFF -D GLFW_BUILD_DOCS=OFF

:: Debug build
echo [%LOG_CHANNEL%] Started building GLFW debug...
cmake --build %GLFW_OUT% --config Debug
echo [%LOG_CHANNEL%] Finished building GLFW debug!

:: Release build
echo [%LOG_CHANNEL%] Started building GLFW release...
cmake --build %GLFW_OUT% --config Release
echo [%LOG_CHANNEL%] Finished building GLFW release!

echo [%LOG_CHANNEL%] Finished building GLFW!
::-----------------------------------------------------------

:: Build glslang
::-----------------------------------------------------------
echo [%LOG_CHANNEL%] Building glslang dependency...

echo [%LOG_CHANNEL%] Pulling glslang dependencies...

set GLSLANG_SRC=".\PHOENIX\vendor\glslang"
set GLSLANG_OUT="%LIB_OUTPUT_DIR%glslang"

:: Push to glslang directory because python script looks for other files using relative paths from the project root
pushd "PHOENIX/vendor/glslang"
py ./update_glslang_sources.py
popd

echo [%LOG_CHANNEL%] Finished pulling glslang dependencies!

:: Build glslang project
cmake -S %GLSLANG_SRC% -B %GLSLANG_OUT% -D GLSLANG_TESTS=OFF

:: Debug build
echo [%LOG_CHANNEL%] Started building glslang debug...
cmake --build %GLSLANG_OUT% --config Debug
echo [%LOG_CHANNEL%] Finished building glslang debug!

:: Release build
echo [%LOG_CHANNEL%] Started building glslang release...
cmake --build %GLSLANG_OUT% --config Release
echo [%LOG_CHANNEL%] Finished building glslang release!

echo [%LOG_CHANNEL%] Finished building glslang dependency!
::-----------------------------------------------------------

echo [%LOG_CHANNEL%] Finished building dependencies!
pause