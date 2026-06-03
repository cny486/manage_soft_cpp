@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0expand_and_launch.ps1"
exit /b %ERRORLEVEL%
