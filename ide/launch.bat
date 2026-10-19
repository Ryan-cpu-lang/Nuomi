@echo off
setlocal
rem ============================================================
rem  Nuomi IDE 启动器  ——  双击即可运行（无需安装、零成本）
rem  1) 确保糯米工具链已编译（nuomi.exe / nucutil.exe）
rem  2) 用本机 Node 启动本地服务并自动打开浏览器
rem ============================================================
cd /d %~dp0

set "NODE=C:\Users\RAYN\.workbuddy\binaries\node\versions\22.22.2-6\node.exe"
if not exist "%NODE%" set "NODE=node.exe"

rem ---- 1. 确保工具链已编译 ----
if not exist "..\nuomi.exe" (
  echo [IDE] 未找到 nuomi.exe，正在编译工具链...
  call "..\build.bat"
)
if not exist "..\nuomi.exe" (
  echo [IDE] 编译失败：请确认已安装 clang/gcc/tcc 之一，然后双击本文件重试。
  pause
  exit /b 1
)

rem ---- 2. 启动 IDE 服务（会自带浏览器） ----
echo [IDE] 启动 Nuomi IDE ...
"%NODE%" "%~dp0server.js"
endlocal
