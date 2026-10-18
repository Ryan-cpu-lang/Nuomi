# SPDX-License-Identifier: GPL-2.0
# tools/make-shortcuts.ps1 -- create Windows shortcuts (.lnk) that carry the Mochi icon.
# Usage (repo root):  powershell -ExecutionPolicy Bypass -File tools\make-shortcuts.ps1
# Creates: Mochi - Games / Mochi - Guess / Mochi - REPL  (double-click to run with mochi.ico icon)
# Note: TCC-built mochi.exe can't embed an icon (no windres / no PE header slack), so we
#       put the Mochi icon on the shortcuts instead. Shortcuts pin to this repo's absolute path;
#       re-run this script if you move the repo. (Shortcuts (*.lnk) are machine-local, git-ignored.)
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$exe  = Join-Path $repo "mochi.exe"
$ico  = Join-Path $repo "mochi.ico"
if (-not (Test-Path $exe)) { Write-Error "mochi.exe not found in $repo - build first (build.bat)"; exit 1 }
if (-not (Test-Path $ico)) { Write-Error "mochi.ico not found in $repo - generate: python tools\gen-icon.py"; exit 1 }

$ws = New-Object -ComObject WScript.Shell
$links = @(
  @{ Name = "Mochi - Games"; Args = "samples\games.mochi";      Desc = "Mochi games: auto battle + number guess" },
  @{ Name = "Mochi - Guess"; Args = "samples\guess-play.mochi"; Desc = "Mochi number-guessing game (you play)" },
  @{ Name = "Mochi - REPL";  Args = "";                          Desc = "Mochi language REPL" }
)
foreach ($l in $links) {
  $path = Join-Path $repo ($l.Name + ".lnk")
  $sc = $ws.CreateShortcut($path)
  $sc.TargetPath       = $exe
  $sc.Arguments        = $l.Args
  $sc.WorkingDirectory = $repo
  $sc.IconLocation     = ($ico + ",0")
  $sc.Description      = $l.Desc
  $sc.Save()
  Write-Host ("created: {0}" -f $path)
}
