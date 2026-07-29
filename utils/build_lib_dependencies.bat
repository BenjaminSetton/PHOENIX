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

:: Build Slang
::-----------------------------------------------------------
echo [%LOG_CHANNEL%] Building Slang dependency...

set SLANG_SRC=".\PHOENIX\vendor\slang"
set SLANG_OUT="%LIB_OUTPUT_DIR%slang"

:: Initialize Slang submodules
echo [%LOG_CHANNEL%] Pulling Slang dependencies...
git -C PHOENIX/vendor/slang submodule update --init --recursive
echo [%LOG_CHANNEL%] Finished pulling Slang dependencies!

:: Build Slang project
cmake --fresh -S %SLANG_SRC% -B %SLANG_OUT% ^
  -D SLANG_LIB_TYPE=SHARED ^
  -D SLANG_ENABLE_TESTS=OFF ^
  -D SLANG_ENABLE_EXAMPLES=OFF ^
  -D SLANG_ENABLE_SLANGD=OFF ^
  -D SLANG_ENABLE_SLANGC=OFF ^
  -D SLANG_ENABLE_SLANGI=OFF ^
  -D SLANG_ENABLE_SLANGRT=OFF ^
  -D SLANG_ENABLE_GFX=OFF ^
  -D SLANG_ENABLE_SLANG_PROXY=OFF ^
  -D SLANG_ENABLE_SLANG_RHI=OFF ^
  -D SLANG_ENABLE_CUDA=OFF ^
  -D SLANG_ENABLE_OPTIX=OFF ^
  -D SLANG_ENABLE_NVAPI=OFF ^
  -D SLANG_ENABLE_AFTERMATH=OFF ^
  -D SLANG_ENABLE_DXIL=OFF ^
  -D SLANG_ENABLE_SLANG_GLSLANG=ON ^
  -D CMAKE_CONFIGURATION_TYPES="Debug;Release" ^
  -D CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG="%LIB_OUTPUT_DIR%slang/bin/windows/debug/x86_64" ^
  -D CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE="%LIB_OUTPUT_DIR%slang/bin/windows/release/x86_64" ^
  -D CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG="%LIB_OUTPUT_DIR%slang/bin/windows/debug/x86_64" ^
  -D CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE="%LIB_OUTPUT_DIR%slang/bin/windows/release/x86_64" ^
  -D CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG="%LIB_OUTPUT_DIR%slang/bin/windows/debug/x86_64" ^
  -D CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE="%LIB_OUTPUT_DIR%slang/bin/windows/release/x86_64" ^
  -D CMAKE_DEBUG_POSTFIX=d ^
  -D CMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"

:: Debug build (bootstrap first, then full build)
echo [%LOG_CHANNEL%] Started building Slang debug...
cmake --build %SLANG_OUT% --config Debug --parallel --target slang-bootstrap slang-without-embedded-core-module
cmake --build %SLANG_OUT% --config Debug --parallel --target slang slang-embedded-core-module-source slang-glslang slang-glsl-module
echo [%LOG_CHANNEL%] Finished building Slang debug!

:: Release build
echo [%LOG_CHANNEL%] Started building Slang release...
cmake --build %SLANG_OUT% --config Release --parallel --target slang-bootstrap slang-without-embedded-core-module
cmake --build %SLANG_OUT% --config Release --parallel --target slang slang-embedded-core-module-source slang-glslang slang-glsl-module
echo [%LOG_CHANNEL%] Finished building Slang release!

echo [%LOG_CHANNEL%] Finished building Slang dependency!
::-----------------------------------------------------------

echo [%LOG_CHANNEL%] Finished building dependencies!