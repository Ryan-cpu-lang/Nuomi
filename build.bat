@echo off
setlocal
set CC=clang
where clang >nul 2>nul || set CC=gcc
if "%CC%"=="gcc" (where gcc >nul 2>nul || (echo 未找到 clang 或 gcc，请先安装 LLVM/clang 或 MinGW-w64 & exit /b 1))
echo 使用编译器: %CC%
%CC% -O2 -Iinclude src/value.c src/reader.c src/env.c src/eval.c src/native.c src/main.c -o mochi.exe
if errorlevel 1 (echo mochi 编译失败 & exit /b 1)
%CC% -O2 -Iinclude src/nucfmt.c src/nucutil.c -o nucutil.exe
if errorlevel 1 (echo nucutil 编译失败 & exit /b 1)
echo 构建完成: mochi.exe, nucutil.exe
endlocal
