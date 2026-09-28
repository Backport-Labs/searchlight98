# Builds Searchlight 98: compiles SLIGHT98.EXE, runs its self-test and packs
# dist\SL98SETUP.EXE.
#
# Needs two free tools (not included in this repository):
#   Tiny C Compiler 0.9.27, 32-bit Windows build
#     http://download.savannah.gnu.org/releases/tinycc/tcc-0.9.27-win32-bin.zip
#   Inno Setup 5.4.3 (non-Unicode; the last version supporting Windows 95/98)
#     https://files.jrsoftware.org/is/5/isetup-5.4.3.exe
#
# Usage:
#   .\build\build.ps1 -Tcc C:\tools\tcc\tcc.exe -Iscc "C:\tools\Inno Setup 5\ISCC.exe"
param(
    [Parameter(Mandatory = $true)] [string] $Tcc,
    [Parameter(Mandatory = $true)] [string] $Iscc
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root 'build\out'
$src = Join-Path $root 'src'
New-Item -ItemType Directory -Force $out | Out-Null

if (-not (Test-Path (Join-Path $src 'icon.h'))) {
    & (Join-Path $PSScriptRoot 'make-art.ps1') -Root $root
}

Write-Host 'Compiling SLIGHT98.EXE...'
$exe = Join-Path $out 'SLIGHT98.EXE'
& $Tcc -mwindows -Wall -o $exe (Join-Path $src 'slight98.c') `
    (Join-Path $src 'shell32.def') (Join-Path $src 'advapi32.def') (Join-Path $src 'ole32.def') `
    -luser32 -lkernel32 -lgdi32
if ($LASTEXITCODE -ne 0) { throw 'Compiling SLIGHT98.EXE failed.' }

Write-Host 'Running the self-test...'
$test = Join-Path $out 'selftest'
New-Item -ItemType Directory -Force $test | Out-Null
Copy-Item $exe $test -Force
Copy-Item (Join-Path $root 'tests\TESTS.TXT') $test -Force
$p = Start-Process (Join-Path $test 'SLIGHT98.EXE') -ArgumentList '/selftest' -PassThru
if (-not $p.WaitForExit(30000)) { Stop-Process -Id $p.Id -Force; throw 'The self-test did not finish.' }
$expected = @(Get-Content (Join-Path $root 'tests\EXPECTED.OUT') -Encoding Default)
$got = @(Get-Content (Join-Path $test 'SELFTEST.OUT') -Encoding Default)
$bad = 0
for ($i = 0; $i -lt $expected.Count; $i++) {
    if ($expected[$i] -ne $got[$i]) { $bad++; Write-Host "  expected: $($expected[$i])`n  got:      $($got[$i])" }
}
if ($bad) { throw "Self-test: $bad of $($expected.Count) lines differ." }
Write-Host "  $($expected.Count) checks passed."

Write-Host 'Building SL98SETUP.EXE...'
& $Iscc /Q (Join-Path $root 'installer\SLIGHT98.ISS')
if ($LASTEXITCODE -ne 0) { throw 'Building the installer failed.' }

Get-Item $exe, (Join-Path $root 'dist\SL98SETUP.EXE') | Select-Object Name, Length
