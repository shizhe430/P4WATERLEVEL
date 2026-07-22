[CmdletBinding()]
param(
    [string]$Port = ''
)

. "$PSScriptRoot\idf-env.ps1"
Set-Location (Split-Path -Parent $PSScriptRoot)

$args = @()
if ($Port) { $args += @('-p', $Port) }
$args += @('-b', '460800', 'flash')
& idf.py @args
exit $LASTEXITCODE
