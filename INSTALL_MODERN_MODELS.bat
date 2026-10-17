@echo off
setlocal
title Liberty City VR - Download Build and Install Modern / Xbox Models
cd /d "%~dp0"

echo ================================================================
echo  LIBERTY CITY VR - DOWNLOAD, BUILD AND INSTALL MODERN / XBOX MODELS
echo ================================================================
echo.

set "INSTALLER=%~dp0tools\prepare-modern-models.ps1"
if not exist "%INSTALLER%" (
    echo ERROR: Required file is missing:
    echo   %INSTALLER%
    echo.
    echo Extract the complete source-kit ZIP before running this file.
    set "RESULT=1"
    goto :finish
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%INSTALLER%" %*
set "RESULT=%ERRORLEVEL%"

:finish
echo.
if not "%RESULT%"=="0" (
    echo FAILED. Read the error above; the previous overlay folder was not replaced silently.
) else (
    echo COMPLETE. Fully restart Liberty City VR on the headset.
)
echo.
echo This window will remain open. Press any key when you are done reading it.
pause >nul
exit /b %RESULT%
