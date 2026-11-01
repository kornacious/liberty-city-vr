@echo off
call "%~dp0MANAGE.bat" --page RepairGameFiles %*
exit /b %ERRORLEVEL%
