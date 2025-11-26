@echo off
REM Build script for AC2 Windows Installer
REM This script compiles the Inno Setup script to create the installer

echo ====================================
echo AC2 Installer Builder
echo ====================================
echo.

REM Check if Inno Setup is installed in default locations
set "INNO_PATH="

if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" (
    set "INNO_PATH=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
)
if exist "C:\Program Files\Inno Setup 6\ISCC.exe" (
    if not defined INNO_PATH set "INNO_PATH=C:\Program Files\Inno Setup 6\ISCC.exe"
)
if exist "C:\Program Files (x86)\Inno Setup 5\ISCC.exe" (
    if not defined INNO_PATH set "INNO_PATH=C:\Program Files (x86)\Inno Setup 5\ISCC.exe"
)
if exist "C:\Program Files\Inno Setup 5\ISCC.exe" (
    if not defined INNO_PATH set "INNO_PATH=C:\Program Files\Inno Setup 5\ISCC.exe"
)

if not defined INNO_PATH (
    echo ERROR: Inno Setup not found!
    echo.
    echo Please install Inno Setup from:
    echo https://jrsoftware.org/isdl.php
    echo.
    echo Install it to the default location, then run this script again.
    echo.
    pause
    exit /b 1
)

echo Found Inno Setup: %INNO_PATH%
echo.
echo Compiling installer...
echo This may take a few minutes depending on the size of your game files.
echo.

"%INNO_PATH%" "AC2_Installer.iss"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ====================================
    echo SUCCESS! Installer built successfully.
    echo ====================================
    echo.
    echo Your installer is located at:
    echo installer_output\AC2_Setup.exe
    echo.
    echo You can now share this file with your friends!
    echo.
    
    REM Open the output folder
    if exist "installer_output\AC2_Setup.exe" (
        explorer /select,"installer_output\AC2_Setup.exe"
    )
) else (
    echo.
    echo ====================================
    echo ERROR: Build failed!
    echo ====================================
    echo.
    echo Please check the error messages above.
    echo.
)

pause

