@echo off
setlocal enabledelayedexpansion

if "%1"=="" goto help

if "%1"=="help" goto help
if "%1"=="docs" goto docs
if "%1"=="show-doc" goto showdoc

if "%1"=="tests" goto tests
if "%1"=="tests-unit" goto tests_unit
if "%1"=="tests-integration" goto tests_integration

if "%1"=="tests-cov" goto tests_cov
if "%1"=="tests-cov-unit" goto tests_cov_unit
if "%1"=="tests-cov-integration" goto tests_cov_integration

if "%1"=="build" goto build
if "%1"=="release" goto release
if "%1"=="dev" goto dev

echo Unknown command: %1
echo Run: make help
echo   build <VERSION>        Debug build via CMake
echo   dev <VERSION>          Debug build via CMake with development settings

exit /b 1

:help
echo.
echo Available commands:
echo   build <VERSION>             Debug build via CMake
echo   dev <VERSION>               Debug build via CMake with development settings
echo   release                     Release build via CMake
echo   docs                        Build documentation
echo   show-doc                    Open documentation in browser
echo   tests                       Run all tests
echo   tests-unit                  Run unit tests
echo   tests-integration           Run integration tests
echo   tests-cov                   Run all tests with coverage
echo   tests-cov-unit              Run unit tests with coverage
echo   tests-cov-integration       Run integration tests with coverage
exit /b 0

:docs
cd docs
call make html
exit /b 0

:showdoc
start docs\build\html\index.html
exit /b 0

:tests
call make.bat tests-unit
call make.bat tests-integration
exit /b 0

:tests_unit
set PYTHONPATH=%CD%\src;%PYTHONPATH%
mayapy tests\unit\invoke.py
exit /b 0

:tests_integration
set PYTHONPATH=%CD%\src;%PYTHONPATH%
mayapy tests\integration\invoke.py
exit /b 0

:tests_cov
mayapy -m coverage erase
call make.bat tests-cov-unit
call make.bat tests-cov-integration
mayapy -m coverage report
exit /b 0

:tests_cov_unit
set PYTHONPATH=%CD%\src;%PYTHONPATH%
mayapy -m coverage run tests\unit\invoke.py
exit /b 0

:tests_cov_integration
set PYTHONPATH=%CD%\src;%PYTHONPATH%
mayapy -m coverage run tests\integration\invoke.py
exit /b 0

:build
if "%2"=="" goto missing_version

python package/package.py --build %2
exit /b 0

:dev
python package/package.py --dev %2
exit /b 0

:release

python package/package.py --release
exit /b 0

:missing_version
echo.
echo ERROR: VERSION is required.
echo Usage:
echo   make.bat build 2026
echo   make.bat release 2026
exit /b 1
