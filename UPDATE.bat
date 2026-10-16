@echo off
setlocal EnableExtensions DisableDelayedExpansion
title Liberty City VR - Update
echo.
echo LIBERTY CITY VR - UPDATE FROM GIT AND INSTALL
echo Reuses downloaded tools. Updates only the APK, preserving game data.
echo Close the game on your Quest before updating.
echo.
if not exist "%~dp0tools\update-and-install.ps1" (
  echo ERROR: Keep UPDATE.bat inside the complete Liberty City VR source kit.
  set "LCVR_UPDATE_EXIT=1"
  goto finished
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\update-and-install.ps1" %*
set "LCVR_UPDATE_EXIT=%ERRORLEVEL%"
:finished
echo.
if not "%LCVR_UPDATE_EXIT%"=="0" echo UPDATE FAILED. Read the error above; the game was not launched.
echo Press any key to close this window.
pause >nul
exit /b %LCVR_UPDATE_EXIT%
