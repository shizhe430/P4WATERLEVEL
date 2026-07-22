[CmdletBinding()]
param()

. "$PSScriptRoot\idf-env.ps1"
Set-Location (Split-Path -Parent $PSScriptRoot)
$env:CCACHE_DISABLE = '1'
idf.py build
exit $LASTEXITCODE
