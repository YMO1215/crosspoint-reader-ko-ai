@echo off
setlocal

cd /d C:\Users\GMK\Downloads\repo_compare\ko || exit /b 1

set FIXNAME=%~n0
set TAG=v1.2.0-ko-%FIXNAME%

echo Using tag: %TAG%
set /p COMMITMSG=Enter commit message: 
if "%COMMITMSG%"=="" exit /b 1

git add lib/Epub/Epub/ParsedText.cpp || exit /b 1
git commit -m "%COMMITMSG%" || exit /b 1
git push origin rebuild/1.2.0-ko || exit /b 1
git tag %TAG% || exit /b 1
git push origin %TAG% || exit /b 1

echo Done: %TAG%
pause
