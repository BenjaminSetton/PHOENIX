@echo off

call "%~dp0config.bat"

cd %WORKSPACE_DIR%

set LOG_CHANNEL=PHOENIX

:: Build GLFW
::-----------------------------------------------------------
echo [%LOG_CHANNEL%] Building GLFW dependency...

set GLFW_SRC=".\PHOENIX\vendor\glfw"
set GLFW_OUT="%LIB_OUTPUT_DIR%glfw"

:: Build GLFW project
cmake --fresh -S %GLFW_SRC% -B %GLFW_OUT% ^
  -D GLFW_BUILD_EXAMPLES=OFF ^
  -D GLFW_BUILD_TESTS=OFF ^
  -D GLFW_BUILD_DOCS=OFF ^
  -D CMAKE_CONFIGURATION_TYPES="Debug;Release;Sanitizer" ^
  -D CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG="%LIB_OUTPUT_DIR%glfw/bin/windows/debug/x86_64" ^
  -D CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE="%LIB_OUTPUT_DIR%glfw/bin/windows/release/x86_64" ^
  -D CMAKE_ARCHIVE_OUTPUT_DIRECTORY_SANITIZER="%LIB_OUTPUT_DIR%glfw/bin/windows/sanitizer/x86_64" ^
  -D CMAKE_DEBUG_POSTFIX=d ^
  -D CMAKE_SANITIZER_POSTFIX=d ^
  -D CMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<CONFIG:Sanitizer>:Debug>DLL"

:: Debug build
echo [%LOG_CHANNEL%] Started building GLFW debug...
cmake --build %GLFW_OUT% --config Debug --parallel
echo [%LOG_CHANNEL%] Finished building GLFW debug!

:: Release build
echo [%LOG_CHANNEL%] Started building GLFW release...
cmake --build %GLFW_OUT% --config Release --parallel
echo [%LOG_CHANNEL%] Finished building GLFW release!

:: Sanitizer build (no ASAN for GLFW since it's C-only)
echo [%LOG_CHANNEL%] Started building GLFW sanitizer...
cmake --build %GLFW_OUT% --config Sanitizer --parallel
echo [%LOG_CHANNEL%] Finished building GLFW sanitizer!

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
cmake --fresh -S %GLSLANG_SRC% -B %GLSLANG_OUT% ^
  -D GLSLANG_TESTS=OFF ^
  -D ENABLE_GLSLANG_BINARIES=OFF ^
  -D ENABLE_HLSL=OFF ^
  -D ENABLE_OPT=OFF ^
  -D CMAKE_CONFIGURATION_TYPES="Debug;Release;Sanitizer" ^
  -D CMAKE_C_FLAGS_SANITIZER="/fsanitize=address /Zi" ^
  -D CMAKE_CXX_FLAGS_SANITIZER="/fsanitize=address /Zi" ^
  -D CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG="%LIB_OUTPUT_DIR%glslang/bin/windows/debug/x86_64" ^
  -D CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE="%LIB_OUTPUT_DIR%glslang/bin/windows/release/x86_64" ^
  -D CMAKE_ARCHIVE_OUTPUT_DIRECTORY_SANITIZER="%LIB_OUTPUT_DIR%glslang/bin/windows/sanitizer/x86_64" ^
  -D CMAKE_SANITIZER_POSTFIX=d ^
  -D CMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<CONFIG:Sanitizer>:Debug>DLL"

:: Debug build
echo [%LOG_CHANNEL%] Started building glslang debug...
cmake --build %GLSLANG_OUT% --config Debug --parallel
echo [%LOG_CHANNEL%] Finished building glslang debug!

:: Release build
echo [%LOG_CHANNEL%] Started building glslang release...
cmake --build %GLSLANG_OUT% --config Release --parallel
echo [%LOG_CHANNEL%] Finished building glslang release!

:: Sanitizer build (ASAN)
echo [%LOG_CHANNEL%] Started building glslang sanitizer...
cmake --build %GLSLANG_OUT% --config Sanitizer --parallel
echo [%LOG_CHANNEL%] Finished building glslang sanitizer!

echo [%LOG_CHANNEL%] Finished building glslang dependency!
::-----------------------------------------------------------

echo [%LOG_CHANNEL%] Finished building dependencies!