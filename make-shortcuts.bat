@echo off
rem Create Mochi icon'd Windows shortcuts (Games / Guess / REPL) using mochi.ico.
rem Double-click this file; it runs tools\make-shortcuts.ps1 (needs mochi.exe + mochi.ico present).
powershell -ExecutionPolicy Bypass -File "%~dp0tools\make-shortcuts.ps1"
pause
