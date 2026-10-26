# Build the pinned Xbox Version HD (re3) assets using the shared builder.
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Source,
      [Parameter(Mandatory=$true)][string]$Out,
      [switch]$Force)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'xbox-modelset.ps1')
$arguments = @('build', '--source', $Source, '--out', $Out, '--profile', 'modern')
if ($Force) { $arguments += '--force' }
Invoke-ModelsetPython -Arguments $arguments
