[CmdletBinding()]
param(
    [string]$Port = ''
)

. "$PSScriptRoot\idf-env.ps1"
Set-Location (Split-Path -Parent $PSScriptRoot)
$env:CCACHE_DISABLE = '1'

$args = @('build')
if ($Port) { $args += @('-p', $Port) }
$args += @('-b', '460800', 'flash', 'monitor')
& idf.py @args
exit $LASTEXITCODE
