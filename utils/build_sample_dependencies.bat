@echo off

call "%~dp0config.bat"

cd %WORKSPACE_DIR%

set LOG_CHANNEL=SAMPLE

:: Build assimp
::-----------------------------------------------------------
echo [%LOG_CHANNEL%] Building assimp dependency...

set ASSIMP_SRC=".\samples\common\vendor\assimp"
set ASSIMP_OUT="%SAMPLES_OUTPUT_DIR%assimp"

:: Build assimp project with minimal importers, no exporters, no tools
cmake --fresh -S %ASSIMP_SRC% -B %ASSIMP_OUT% ^
  -D BUILD_SHARED_LIBS=OFF ^
  -D ASSIMP_BUILD_TESTS=OFF ^
  -D ASSIMP_BUILD_ZLIB=ON ^
  -D ASSIMP_BUILD_ASSIMP_TOOLS=OFF ^
  -D ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT=OFF ^
  -D ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=OFF ^
  -D ASSIMP_BUILD_FBX_IMPORTER=ON ^
  -D ASSIMP_BUILD_GLTF_IMPORTER=ON ^
  -D ASSIMP_BUILD_GLTF2_IMPORTER=ON ^
  -D ASSIMP_BUILD_OBJ_IMPORTER=ON ^
  -D LIBRARY_SUFFIX="-mt" ^
  -D CMAKE_CONFIGURATION_TYPES="Debug;Release;Sanitizer" ^
  -D CMAKE_C_FLAGS_SANITIZER="/fsanitize=address /Zi" ^
  -D CMAKE_CXX_FLAGS_SANITIZER="/fsanitize=address /Zi" ^
  -D CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG="%SAMPLES_OUTPUT_DIR%assimp/bin/windows/debug/x86_64" ^
  -D CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE="%SAMPLES_OUTPUT_DIR%assimp/bin/windows/release/x86_64" ^
  -D CMAKE_ARCHIVE_OUTPUT_DIRECTORY_SANITIZER="%SAMPLES_OUTPUT_DIR%assimp/bin/windows/sanitizer/x86_64" ^
  -D CMAKE_SANITIZER_POSTFIX=d ^
  -D CMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<CONFIG:Sanitizer>:Debug>DLL"

:: Debug build
echo [%LOG_CHANNEL%] Started building assimp debug...
cmake --build %ASSIMP_OUT% --config Debug --parallel
echo [%LOG_CHANNEL%] Finished building assimp debug!

:: Release build
echo [%LOG_CHANNEL%] Started building assimp release...
cmake --build %ASSIMP_OUT% --config Release --parallel
echo [%LOG_CHANNEL%] Finished building assimp release!

:: Sanitizer build (ASAN)
echo [%LOG_CHANNEL%] Started building assimp sanitizer...
cmake --build %ASSIMP_OUT% --config Sanitizer --parallel
echo [%LOG_CHANNEL%] Finished building assimp sanitizer!

echo [%LOG_CHANNEL%] Finished building assimp!
::-----------------------------------------------------------

:: Build glm
::-----------------------------------------------------------
echo [%LOG_CHANNEL%] Building glm dependency...

set GLM_SRC=".\samples\common\vendor\glm"
set GLM_OUT="%SAMPLES_OUTPUT_DIR%glm"

:: Build glm project
cmake --fresh -S %GLM_SRC% -B %GLM_OUT% ^
  -D GLM_BUILD_LIBRARY=ON ^
  -D GLM_BUILD_TESTS=OFF ^
  -D CMAKE_CONFIGURATION_TYPES="Debug;Release;Sanitizer" ^
  -D CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG="%SAMPLES_OUTPUT_DIR%glm/bin/windows/debug/x86_64" ^
  -D CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE="%SAMPLES_OUTPUT_DIR%glm/bin/windows/release/x86_64" ^
  -D CMAKE_ARCHIVE_OUTPUT_DIRECTORY_SANITIZER="%SAMPLES_OUTPUT_DIR%glm/bin/windows/sanitizer/x86_64" ^
  -D CMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<CONFIG:Sanitizer>:Debug>DLL"

:: Debug build
echo [%LOG_CHANNEL%] Started building glm debug...
cmake --build %GLM_OUT% --config Debug --parallel
echo [%LOG_CHANNEL%] Finished building glm debug!

:: Release build
echo [%LOG_CHANNEL%] Started building glm release...
cmake --build %GLM_OUT% --config Release --parallel
echo [%LOG_CHANNEL%] Finished building glm release!

:: Sanitizer build
echo [%LOG_CHANNEL%] Started building glm sanitizer...
cmake --build %GLM_OUT% --config Sanitizer --parallel
echo [%LOG_CHANNEL%] Finished building glm sanitizer!

echo [%LOG_CHANNEL%] Finished building glm!
::-----------------------------------------------------------

echo [%LOG_CHANNEL%] Finished building dependencies!