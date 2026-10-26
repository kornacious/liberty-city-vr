# Modern and Xbox now use the same supported GTA III re3 source pack.
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Source,
      [Parameter(Mandatory=$true)][string]$Out,
      [switch]$Force)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'xbox-modelset.ps1')
$arguments = @('build', '--source', $Source, '--out', $Out, '--profile', 'modern')
if ($Force) { $arguments += '--force' }
Invoke-ModelsetPython -Arguments $arguments
