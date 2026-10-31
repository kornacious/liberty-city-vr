param([string]$OutDir=(Join-Path $PSScriptRoot '../../.cache/world-lod-tests'))
$ErrorActionPreference='Stop'
$root=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$source=[IO.File]::ReadAllText((Join-Path $root 'overlay/src/renderer/VrWorldLod.cpp'))
$source=[regex]::Replace($source,'(?m)^#include[^\r\n]*\r?\n','')
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'world-lod-production.inc'),$source)
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -OutDir $OutDir -Sources @('test-world-lod.cpp','test-perf-capture.cpp')
if($LASTEXITCODE -ne 0){throw 'World LOD tests failed'}
