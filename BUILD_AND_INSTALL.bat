@echo off
call "%~dp0MANAGE.bat" --page BuildAndInstall %*
exit /b %ERRORLEVEL%
