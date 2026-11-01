@echo off
call "%~dp0MANAGE.bat" --page Update %*
exit /b %ERRORLEVEL%
