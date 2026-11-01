@echo off
setlocal
if exist "%~dp0out\manager-windows\LibertyManager.exe" (
  "%~dp0out\manager-windows\LibertyManager.exe" %*
) else (
  dotnet run --project "%~dp0tools\manager\LibertyManager.csproj" -c Release -- %*
)
if errorlevel 1 pause