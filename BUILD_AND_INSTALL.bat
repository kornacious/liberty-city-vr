@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem A double-click normally starts cmd.exe /c, which can disappear before a
rem user can read an early host/security error. Run the real wizard inside a
rem nested /k shell. That shell remains open even if PowerShell is terminated.
if not defined LCVR_KEEP_OPEN (
  set "LCVR_KEEP_OPEN=1"
  "%ComSpec%" /d /k call "%~f0" %*
  exit /b
)

title Liberty City VR - Personal Quest Builder

echo.
echo ============================================================
echo   LIBERTY CITY VR - PERSONAL QUEST BUILD AND INSTALL WIZARD
echo ============================================================
echo.
echo This builds a personal APK from source. No APK or GTA data is
echo included in this repository.
echo.

set "LCVR_SCRIPT=%~dp0tools\build-and-install.ps1"
set "LCVR_LOG=%TEMP%\LibertyCityVR-Build-And-Install.log"

where powershell.exe >nul 2>&1
if errorlevel 1 (
  echo ERROR: Windows PowerShell was not found.
  set "LCVR_EXIT=1"
  goto :finished
)

if not exist "%LCVR_SCRIPT%" (
  echo ERROR: Required file is missing: %LCVR_SCRIPT%
  echo Please make sure the complete repository is present.
  set "LCVR_EXIT=1"
  goto :finished
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%LCVR_SCRIPT%" -LogPath "%LCVR_LOG%" %*
set "LCVR_EXIT=%ERRORLEVEL%"

:finished
echo.
if not "%LCVR_EXIT%"=="0" (
  echo FAILED. Read the error above; nothing was silently ignored.
) else (
  echo SUCCESS. The wizard completed all requested steps.
)
echo.
echo Diagnostic log:
echo   %LCVR_LOG%
echo.
echo This window will remain open. Press any key when you are done reading it.
pause >nul
exit /b %LCVR_EXIT%
