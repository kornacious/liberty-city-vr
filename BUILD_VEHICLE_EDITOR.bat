@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\vehicle-editor\build.ps1" %*
exit /b %ERRORLEVEL%
