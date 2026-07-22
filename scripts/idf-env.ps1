[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$idfPath = 'C:\Espressif\frameworks\esp-idf-v5.5.5'
$toolsPath = 'C:\Espressif'
$pythonEnv = 'C:\Espressif\python_env\idf5.5_py3.11_env'
$gitBin = 'C:\Espressif\tools\idf-git\2.44.0\cmd'

foreach ($path in @($idfPath, $pythonEnv, $gitBin)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "ESP-IDF environment path does not exist: $path"
    }
}

$env:IDF_TOOLS_PATH = $toolsPath
$env:IDF_PATH = $idfPath
$env:IDF_PYTHON_ENV_PATH = $pythonEnv
$env:Path = "$pythonEnv\Scripts;$gitBin;$env:Path"

$previousErrorAction = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
. "$idfPath\export.ps1"
$exportExitCode = $LASTEXITCODE
$ErrorActionPreference = $previousErrorAction
if ($exportExitCode -ne 0) {
    throw "ESP-IDF environment activation failed with exit code $exportExitCode."
}
