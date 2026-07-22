[CmdletBinding()]
param()

. "$PSScriptRoot\idf-env.ps1"
Set-Location (Split-Path -Parent $PSScriptRoot)
idf.py fullclean
exit $LASTEXITCODE
