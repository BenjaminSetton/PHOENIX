@echo off

echo Running first time setup...

:: Dependencies
call "%~dp0build_lib_dependencies.bat"
call "%~dp0build_sample_dependencies.bat"

:: Project
call "%~dp0build_project.bat"

echo Finished first time setup!