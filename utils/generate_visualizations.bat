@echo off

echo Generating render graph visualizations...

:: See more in https://graphviz.org/docs/outputs/
set OUTPUT_FORMAT=svg

set "SAMPLES_DIR=%~dp0..\samples"

for /r "%SAMPLES_DIR%" %%F in (*.dot) do (
    dot -T%OUTPUT_FORMAT% "%%F" -o "%%~dpnF.%OUTPUT_FORMAT%"
    echo Generated "%%~dpnF.%OUTPUT_FORMAT%"
)

pause
