param([string]$KitRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$source=[IO.File]::ReadAllText((Join-Path $KitRoot 'overlay/src/peds/VrPlayerTraversal.cpp'))
$start=$source.IndexOf('namespace VrPlayerTraversal {')
$end=$source.LastIndexOf('#endif')
if($start -lt 0 -or $end -lt $start){throw 'Cannot extract traversal implementation.'}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'player-traversal-production.inc'),$source.Substring($start,$end-$start))
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $KitRoot -Sources @('test-player-traversal.cpp')
