@echo off
call "%~dp0MANAGE.bat" --page InstallModernModels %*
exit /b %ERRORLEVEL%
