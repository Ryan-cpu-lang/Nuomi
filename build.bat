@echo off
chcp 65001 >nul
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

rem nuomi: single exe = interpreter + -c codegen (codegen.c) + NUC read/write (nucfmt.c)
rem + NUC toolchain (nucutil.c, provides "nuomi nuc" subcommand), all linked in.
rem -lkernel32: main.c sets console to UTF-8 on Windows via SetConsoleOutputCP and SetConsoleCP
rem   nucutil.c run also uses VirtualAlloc, so kernel32 is required too.
"%CC%" -O2 -Iinclude -lkernel32 -luser32 -lgdi32 src\value.c src\reader.c src\env.c src\eval.c src\native.c src\json.c src\http.c src\proc.c src\wm.c src\codegen.c src\nucfmt.c src\update.c src\nucutil.c src\main.c src\sha1.c src\zip.c src\strutil.c -o nuomi.exe
if errorlevel 1 (echo [build] nuomi FAILED & exit /b 1)

rem Embed the Nuomi mochi icon into the exe (TCC can't do this natively).
rem Falls back silently if python isn't available so the build still succeeds.
where python >nul 2>nul && python tools\embed-icon.py nuomi.exe nuomi.ico >nul 2>nul && (
  echo [build] icon embedded into nuomi.exe
  if not exist dist\windows mkdir dist\windows
  copy /Y nuomi.exe dist\windows\nuomi.exe >nul
  echo [build] synced dist\windows\nuomi.exe
) || echo [build] (skipped icon embed: python not found)

rem ---- nuomiw.exe: GUI subsystem twin, no console window ----
rem Same binary + icon as nuomi.exe; only the PE Subsystem field is flipped
rem CUI(3) -> GUI(2) by make-windowed.py. Used by the launcher GUI so that
rem double-clicking does not spawn a black cmd box. nuomi.exe stays console.
where python >nul 2>nul && python tools\make-windowed.py nuomi.exe nuomiw.exe && (
  if not exist dist\windows mkdir dist\windows
  copy /Y nuomiw.exe dist\windows\nuomiw.exe >nul
  echo [build] synced dist\windows\nuomiw.exe
) || echo [build] (skipped nuomiw.exe: python not found)

echo [build] OK: nuomi.exe + nuomiw.exe (NUC toolchain included)
endlocal
