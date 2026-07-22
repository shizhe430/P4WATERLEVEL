[CmdletBinding()]
param(
    [string]$Port = ''
)

. "$PSScriptRoot\idf-env.ps1"
Set-Location (Split-Path -Parent $PSScriptRoot)

$args = @()
if ($Port) { $args += @('-p', $Port) }
$args += @('monitor', '-b', '921600')
& idf.py @args
exit $LASTEXITCODE
