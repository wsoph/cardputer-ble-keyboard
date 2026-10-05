param(
    [string]$PackageRoot = (Join-Path $PSScriptRoot 'release-packages'),
    [string]$Python = 'python',
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '.pio\build\cardputer-ble-keyboard'),
    [string]$FrameworkDirectory,
    [string]$CoverPath
)
$ErrorActionPreference = 'Stop'
if (-not $FrameworkDirectory) {
    $coreDirectory = $env:PLATFORMIO_CORE_DIR
    if (-not $coreDirectory) { $coreDirectory = Join-Path ([Environment]::GetFolderPath('UserProfile')) '.platformio' }
    $FrameworkDirectory = Join-Path $coreDirectory 'packages\framework-arduinoespressif32'
}
$packageArguments = @((Join-Path $PSScriptRoot 'tools\package_firmware.py'), '--build-dir', $BuildDirectory,
    '--framework-dir', $FrameworkDirectory, '--package-root', $PackageRoot)
if ($CoverPath) { $packageArguments += @('--cover', $CoverPath) }
& $Python @packageArguments
if ($LASTEXITCODE -ne 0) { throw 'Firmware packaging/validation failed.' }
