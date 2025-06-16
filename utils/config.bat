
:: Generate a Visual Studio solution
:: Visual Studio 2019: "vs2019"
:: Visual Studio 2022: "vs2022"
set GENERATOR="vs2022"

set LIB_OUTPUT_DIR=%WORKSPACE_DIR%PHOENIX\out\
set SAMPLES_OUTPUT_DIR=%WORKSPACE_DIR%samples\common\out\
set WORKSPACE_DIR=%~dp0..\