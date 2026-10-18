@echo off
chcp 65001 >nul
cd /d "%~dp0"
mochi.exe samples\games.mochi
echo.
pause
