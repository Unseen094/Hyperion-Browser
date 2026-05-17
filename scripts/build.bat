@echo off
setlocal

set BUILD_DIR=build
set CONFIG=Debug

if not exist %BUILD_DIR% mkdir %BUILD_DIR%

echo Configuring Hyperion (VS 2026)...
cmake -B %BUILD_DIR% -S . -G "Visual Studio 18 2026" -A x64

if %ERRORLEVEL% neq 0 (
    echo Configuration failed!
    exit /b %ERRORLEVEL%
)

echo Building Hyperion (%CONFIG%)...
cmake --build %BUILD_DIR% --config %CONFIG%

if %ERRORLEVEL% neq 0 (
    echo Build failed!
    exit /b %ERRORLEVEL%
)

echo Build successful!
echo Binary located at: %BUILD_DIR%\bin\%CONFIG%\hyperion.exe
