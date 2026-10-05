param(
    [string]$Python = 'python',
    [string]$CoreDirectory,
    [string]$BuildDirectory,
    [string]$LibraryDirectory
)
$ErrorActionPreference = 'Stop'
$overrides = @{
    PLATFORMIO_CORE_DIR = $CoreDirectory
    PLATFORMIO_BUILD_DIR = $BuildDirectory
    PLATFORMIO_LIBDEPS_DIR = $LibraryDirectory
}
$savedEnvironment = @{}
$savedPreference = $ErrorActionPreference
try {
    foreach ($name in $overrides.Keys) {
        if ($overrides[$name]) {
            $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
            [Environment]::SetEnvironmentVariable($name, $overrides[$name], 'Process')
        }
    }
    $ErrorActionPreference = 'Continue'
    & $Python -m platformio run --project-dir $PSScriptRoot -e cardputer-ble-keyboard
    $nativeExitCode = $LASTEXITCODE
} finally {
    $ErrorActionPreference = $savedPreference
    foreach ($name in $savedEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process')
    }
}
if ($nativeExitCode -ne 0) { throw ('Keyboard build failed with exit code ' + $nativeExitCode) }
