@echo off
cd /d "%~dp0"

REM ====== CONFIG (change for other projects) ======
set REMOTE=origin
set PREFIX=sync/
REM ===============================================

echo ============================================================
echo   Git Sync - Pull Latest
echo ============================================================

echo [1/3] Fetching remote...
git fetch %REMOTE% --prune

echo [2/3] Finding latest sync branch...

set LATEST=
for /f "tokens=*" %%b in ('git branch -r ^| findstr "%REMOTE%/%PREFIX%" ^| sort /R') do (
    if not defined LATEST set "LATEST=%%b"
)

if "%LATEST%"=="" (
    echo No sync branch found. Run git_push_now.bat first.
    pause
    exit /b
)

echo Latest: %LATEST%

for /f "tokens=1,* delims=/" %%a in ("%LATEST%") do set "LOCAL_BRANCH=%%b"

echo [3/3] Overwriting local...

git reset --hard HEAD
git checkout %LOCAL_BRANCH% 2>nul
if errorlevel 1 git checkout -b %LOCAL_BRANCH% %LATEST%
git reset --hard %LATEST%
git clean -fdx

echo.
echo Done! Now on: %LOCAL_BRANCH%
echo.
git log --oneline -5
pause
