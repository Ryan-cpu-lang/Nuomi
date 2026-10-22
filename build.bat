@echo off
setlocal
rem ============================================================
rem  Nuomi + NUC toolchain - Windows build (produces nuomi.exe, nucutil.exe)
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

rem nuomi: interpreter + -c backend (codegen.c) + NUC read/write (nucfmt.c) MUST be linked in.
rem -lkernel32: main.c sets console to UTF-8 on Windows (SetConsoleOutputCP/SetConsoleCP).
"%CC%" -O2 -Iinclude -lkernel32 src\value.c src\reader.c src\env.c src\eval.c src\native.c src\json.c src\http.c src\codegen.c src\nucfmt.c src\main.c -o nuomi.exe
if errorlevel 1 (echo [build] nuomi FAILED & exit /b 1)

rem nucutil: needs kernel32 for VirtualAlloc; -lkernel32 harmless for MinGW/clang, required for tcc.
"%CC%" -O2 -Iinclude -lkernel32 src\nucfmt.c src\nucutil.c -o nucutil.exe
if errorlevel 1 (echo [build] nucutil FAILED & exit /b 1)

echo [build] OK: nuomi.exe , nucutil.exe
endlocal
