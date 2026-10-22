@echo off
where py >nul 2>nul
if not errorlevel 1 (
    py -3 "%~dp0tools\import-music.py" %*
) else (
    python "%~dp0tools\import-music.py" %*
)
if errorlevel 1 pause
