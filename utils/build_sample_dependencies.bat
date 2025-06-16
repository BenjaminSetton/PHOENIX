@echo off

call config.bat

cd %WORKSPACE_DIR%

set LOG_CHANNEL=SAMPLE

:: Build assimp
::-----------------------------------------------------------
echo [%LOG_CHANNEL%] Building assimp dependency...

set ASSIMP_SRC=".\samples\common\vendor\assimp"
set ASSIMP_OUT="%SAMPLES_OUTPUT_DIR%assimp"

:: Build assimp project
cmake -S %ASSIMP_SRC% -B %ASSIMP_OUT% -D BUILD_SHARED_LIBS=OFF -D ASSIMP_BUILD_TESTS=OFF -D ASSIMP_BUILD_ZLIB=ON

:: Debug build
echo [%LOG_CHANNEL%] Started building assimp debug...
cmake --build %ASSIMP_OUT% --config Debug
echo [%LOG_CHANNEL%] Finished building assimp debug!

:: Release build
echo [%LOG_CHANNEL%] Started building assimp release...
cmake --build %ASSIMP_OUT% --config Release
echo [%LOG_CHANNEL%] Finished building assimp release!

echo [%LOG_CHANNEL%] Finished building assimp!
::-----------------------------------------------------------

echo [%LOG_CHANNEL%] Finished building dependencies!
pause