@echo off

echo Running first time setup...

:: Dependencies
call build_lib_dependencies.bat
call build_sample_dependencies.bat

:: Project
call build_solution.bat

echo Finished first time setup!