@echo off
setlocal
rem Windows Terminal consumes touch taps from Virtual Desktop before the TUI sees them.
rem Re-enter once in classic Console Host, without changing the system default terminal.
if defined LCVR_MANAGER_CONHOST goto manager
set "LCVR_MANAGER_CONHOST=1"
start "Liberty City VR Manager" "%SystemRoot%\System32\conhost.exe" "%ComSpec%" /d /s /c ""%~f0" %*"
exit /b %errorlevel%

:manager
set "LCVR_MANAGER_CONHOST="
if exist "%~dp0out\manager-windows\LibertyManager.exe" (
  "%~dp0out\manager-windows\LibertyManager.exe" %*
) else (
  dotnet run --project "%~dp0tools\manager\LibertyManager.csproj" -c Release -- %*
)
if errorlevel 1 pause
