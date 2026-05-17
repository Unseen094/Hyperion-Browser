@echo off
echo ========================================
echo Hyperion Browser Installer Builder
echo ========================================
echo.

REM Check if NSIS is installed
where makensis >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: NSIS not found!
    echo Please install NSIS from: https://nsis.sourceforge.io/
    echo After installing, run this script again.
    pause
    exit /b 1
)

REM Build the installer
echo Building HyperionSetup.exe...
makensis HyperionSetup.nsi

if %ERRORLEVEL% EQU 0 (
    echo.
    echo SUCCESS! Installer created: HyperionSetup.exe
    echo.
) else (
    echo.
    echo ERROR: Failed to build installer
    pause
    exit /b 1
)

pause