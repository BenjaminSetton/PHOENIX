@echo off

echo Running first time setup...

:: Dependencies
powershell -ExecutionPolicy Bypass -File "%~dp0build_lib_dependencies.ps1" -Config All
powershell -ExecutionPolicy Bypass -File "%~dp0build_sample_dependencies.ps1" -Config All

:: Project
powershell -ExecutionPolicy Bypass -File "%~dp0generate_project.ps1"

echo Finished first time setup!