@echo off
setlocal enabledelayedexpansion

rem --- Parse args: extract plugin=NAME and a positional version (first non-plugin token) ---
rem Skips the first token (the command name itself) so the dispatch on %1 stays the source of truth.
rem cmd splits "plugin=NAME" on '=' into two tokens ("plugin" and "NAME"), so the state machine
rem uses the literal token "plugin" to switch into "expect the value next" mode.
set "PLUGIN_NAME="
set "VERSION_ARG="
set "_FIRST=1"
set "_EXPECT_PLUGIN_VALUE="
for %%P in (%*) do (
    set "tok=%%~P"
    if "!_FIRST!"=="1" (
        set "_FIRST=0"
    ) else if defined _EXPECT_PLUGIN_VALUE (
        set "_EXPECT_PLUGIN_VALUE="
        set "PLUGIN_NAME=!tok!"
    ) else if /i "!tok!"=="plugin" (
        set "_EXPECT_PLUGIN_VALUE=1"
    ) else (
        if not defined VERSION_ARG set "VERSION_ARG=!tok!"
    )
)

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
if "%1"=="add-plugin" goto add_plugin

echo Unknown command: %1
echo Run: make help
echo   dev <VERSION>            Dev build (optional version)
echo   dev [VERSION] [plugin=NAME]    Dev build (and deploy) - optionally filtered to one plugin
echo   build <VERSION>        Build debug for specific Maya version
echo   build VERSION [plugin=NAME]    Build (no deploy) - optionally filtered to one plugin
echo   add-plugin <NAME>      Add a new C++ plugin to the project

exit /b 1

:help
echo.
echo Available commands:
echo   dev                        Dev build (builds all Maya versions)
echo   dev <VERSION>               Dev build for specific Maya version
echo   dev [VERSION] [plugin=NAME] Dev build (and deploy) - optionally filtered to one plugin
echo   build <VERSION>            Build debug for specific Maya version
echo   build VERSION [plugin=NAME] Build debug (no deploy) - optionally filtered to one plugin
echo   plugin=NAME is optional. When set, only the named C++ plugin is built.
echo   release                     Release build
echo   add-plugin <NAME>           Add a new C++ plugin to the project
echo   docs                        Build documentation
echo   doctor                      Check environment setup
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
if "!VERSION_ARG!"=="" goto missing_version
if "!PLUGIN_NAME!"=="" (
    python package\package.py --build !VERSION_ARG!
) else (
    python package\package.py --build !VERSION_ARG! --plugin !PLUGIN_NAME!
)
exit /b 0

:dev
if "!PLUGIN_NAME!"=="" (
    python package\package.py --dev !VERSION_ARG!
) else (
    python package\package.py --dev !VERSION_ARG! --plugin !PLUGIN_NAME!
)
exit /b 0

:release

python package/package.py --release
exit /b 0

:missing_version
echo.
echo ERROR: VERSION is required.
echo Usage:
echo   make.bat dev 2024
echo   make.bat dev -v 2024
echo   make.bat dev --version 2024
echo   make.bat build 2024
echo   make.bat build -v 2024
echo   make.bat build --version 2024
exit /b 1

:add_plugin
if "%2"=="" goto missing_plugin_name
python package/package.py --add-plugin %2
exit /b 0

:missing_plugin_name
echo.
echo ERROR: Plugin name is required.
echo Usage:
echo   make.bat add-plugin myPlugin
exit /b 1

:doctor
echo Checking environment...

where mayapy
if errorlevel 1 (
    echo ERROR: mayapy not found in PATH
    exit /b 1
)

python --version

echo Environment looks OK.
exit /b 0