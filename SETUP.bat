@echo off
setlocal
if exist "%~dp0Setup\LibertySetup.exe" (
    start "" "%~dp0Setup\LibertySetup.exe"
    exit /b
)
if exist "%~dp0out\setup-win-x64\LibertySetup.exe" (
    start "" "%~dp0out\setup-win-x64\LibertySetup.exe"
    exit /b
)
where dotnet >nul 2>&1
if errorlevel 1 (
    echo Download the Windows x64 Setup kit from the Liberty City VR releases page.
    pause
    exit /b 1
)
dotnet run --project "%~dp0tools\setup" -c Release --artifacts-path "%~dp0out\setup-local-build"
if errorlevel 1 pause
