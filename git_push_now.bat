@echo off
cd /d "%~dp0"

REM ====== CONFIG (change for other projects) ======
set REMOTE=origin
set PREFIX=sync/
REM ===============================================

for /f "tokens=1-3 delims=/ " %%a in ("%date%") do set d=%%a-%%b-%%c
for /f "tokens=1-2 delims=: " %%a in ("%time%") do set t=%%a-%%b
set t=%t: =0%

set BRANCH=%PREFIX%%d%_%t%

echo ============================================================
echo   Git Sync - Push
echo   Branch: %BRANCH%
echo ============================================================

git checkout -b %BRANCH% 2>nul
if errorlevel 1 git checkout %BRANCH%
git add .
git commit -m "sync: %d% %t%"
git push -u %REMOTE% %BRANCH%

echo.
echo Done! Branch: %BRANCH%
pause
