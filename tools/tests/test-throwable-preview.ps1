param(
    [Parameter(Mandatory=$true)][string]$StageRoot,
    [string]$OutDir = (Join-Path $env:TEMP ('lc-throwable-' + [guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference = 'Stop'
$source = [IO.File]::ReadAllText((Join-Path $StageRoot 'src/core/main.cpp'))
$match = [regex]::Match($source, '(?ms)^static void\r?\nRenderVrThrowableTrajectory\(.*?^\}')
if (-not $match.Success) { throw 'Missing production throwable renderer.' }
$types = '#include "' + ((Join-Path ([IO.Path]::GetFullPath($StageRoot)) 'src/weapons/WeaponType.h') -replace '\\','/') + '"'
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'throwable-preview-production.inc'), $types + "`n" + $match.Value)
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-throwable-preview.cpp')
