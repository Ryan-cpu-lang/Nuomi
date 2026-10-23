@echo off
REM 双击本文件即可安装 Nuomi（零依赖，无需管理员）
REM 卸载请用： powershell -ExecutionPolicy Bypass -File "%~dp0install.ps1" -Uninstall
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1"
pause
