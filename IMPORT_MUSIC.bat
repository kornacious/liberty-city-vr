@echo off
call "%~dp0MANAGE.bat" --page ImportMusic %*
exit /b %ERRORLEVEL%
