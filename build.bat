@echo off
setlocal
rem ============================================================
rem  Nuomi + NUC toolchain - Windows build (produces a single nuomi.exe)
rem  Compiler probe order: clang -> gcc(MinGW-w64) -> tcc(incl. bundled green TCC path)
rem ============================================================
set CC=
where clang >nul 2>nul && set "CC=clang"
if not defined CC (where gcc >nul 2>nul && set "CC=gcc")
if not defined CC (where tcc >nul 2>nul && set "CC=tcc")
if not defined CC (if exist "%LOCALAPPDATA%\tcc\tcc\tcc.exe" set "CC=%LOCALAPPDATA%\tcc\tcc\tcc.exe")

if not defined CC (
  echo [build] ERROR: no C compiler found. Install LLVM/clang, MinGW-w64, or TinyCC.
  exit /b 1
)
echo [build] compiler = %CC%

rem nuomi: 单一可执行文件。解释器 + -c 编译后端 (codegen.c) + NUC 读写 (nucfmt.c)
rem + NUC 工具链 (nucutil.c，提供 nuomi nuc 子命令) 全部链接进这一个 exe。
rem -lkernel32: main.c 在 Windows 上把控制台设为 UTF-8 (SetConsoleOutputCP/SetConsoleCP)；
rem   nucutil.c 的 run 也用到 VirtualAlloc，同样需要 kernel32。
"%CC%" -O2 -Iinclude -lkernel32 src\value.c src\reader.c src\env.c src\eval.c src\native.c src\json.c src\http.c src\proc.c src\codegen.c src\nucfmt.c src\update.c src\nucutil.c src\main.c -o nuomi.exe
if errorlevel 1 (echo [build] nuomi FAILED & exit /b 1)

rem Embed the Nuomi mochi icon into the exe (TCC can't do this natively).
rem Falls back silently if python isn't available so the build still succeeds.
where python >nul 2>nul && python tools\embed-icon.py nuomi.exe nuomi.ico >nul 2>nul && (
  echo [build] icon embedded into nuomi.exe
  if not exist dist\windows mkdir dist\windows
  copy /Y nuomi.exe dist\windows\nuomi.exe >nul
  echo [build] synced dist\windows\nuomi.exe
) || echo [build] (skipped icon embed: python not found)

echo [build] OK: nuomi.exe （单一文件，已含 NUC 工具链）
endlocal
