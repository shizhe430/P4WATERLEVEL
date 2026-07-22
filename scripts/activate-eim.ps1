[CmdletBinding()]
param(
    [switch]$e
)

$ErrorActionPreference = 'Stop'
$idfPath = 'C:\Espressif\frameworks\esp-idf-v5.5.5'
$toolsPath = 'C:\Espressif'
$pythonEnv = 'C:\Espressif\python_env\idf5.5_py3.11_env'
$gitBin = 'C:\Espressif\tools\idf-git\2.44.0\cmd'

if (-not (Test-Path -LiteralPath "$idfPath\export.ps1")) {
    throw "ESP-IDF export script not found: $idfPath\export.ps1"
}

$env:IDF_TOOLS_PATH = $toolsPath
$env:IDF_PATH = $idfPath
$env:IDF_PYTHON_ENV_PATH = $pythonEnv
$env:Path = "$pythonEnv\Scripts;$gitBin;$env:Path"

$previousErrorAction = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
if ($e) {
    . "$idfPath\export.ps1" *> $null
    $exportExitCode = $LASTEXITCODE
    $ErrorActionPreference = $previousErrorAction
    if ($exportExitCode -ne 0) {
        throw "ESP-IDF environment activation failed with exit code $exportExitCode."
    }
    foreach ($name in @(
        'IDF_PATH',
        'IDF_TOOLS_PATH',
        'IDF_PYTHON_ENV_PATH',
        'ESP_IDF_VERSION',
        'OPENOCD_SCRIPTS',
        'PYTHON',
        'PATH'
    )) {
        $value = [Environment]::GetEnvironmentVariable($name, 'Process')
        if ($null -ne $value) {
            "$name=$value"
        }
    }
    exit 0
}

. "$idfPath\export.ps1"
$exportExitCode = $LASTEXITCODE
$ErrorActionPreference = $previousErrorAction
if ($exportExitCode -ne 0) {
    throw "ESP-IDF environment activation failed with exit code $exportExitCode."
}
