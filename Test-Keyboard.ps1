param(
    [string]$Compiler = 'c++',
    [switch]$Zig,
    [string]$BuildDirectory = (Join-Path ([System.IO.Path]::GetTempPath()) 'cardputer-keyboard-tests')
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $BuildDirectory | Out-Null
$exe = Join-Path $BuildDirectory 'keyboard-tests.exe'
$compilerArguments = @('-std=c++17', '-Wall', '-Wextra', '-Werror', '-O1', '-I',
    (Join-Path $PSScriptRoot 'include'), (Join-Path $PSScriptRoot 'test\test_keyboard.cpp'),
    (Join-Path $PSScriptRoot 'src\keyboard_core.cpp'), (Join-Path $PSScriptRoot 'src\pairing_prompt.cpp'), '-o', $exe)
if ($Zig) { $compilerArguments = @('c++') + $compilerArguments }
& $Compiler @compilerArguments
if ($LASTEXITCODE -ne 0) { throw 'Keyboard native test compilation failed.' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'Keyboard native tests failed.' }
