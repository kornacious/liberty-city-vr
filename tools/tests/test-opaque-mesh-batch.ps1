param([string]$OutDir=(Join-Path $PSScriptRoot '../../.cache/opaque-mesh-batch-tests'))
$ErrorActionPreference='Stop'
$root=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$source=[IO.File]::ReadAllText((Join-Path $root 'librw/src/vulkan/vkpipe.cpp'))
$body=[regex]::Match($source,'(?ms)^static bool\r?\nsameOpaqueBatchMaterial\(.*?^\}')
if(-not $body.Success){throw 'Production material compatibility function missing'}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'opaque-mesh-batch-production.inc'),$body.Value)
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -OutDir $OutDir -Sources @('test-opaque-mesh-batch.cpp','test-perf-capture.cpp')
if($LASTEXITCODE -ne 0){throw 'Opaque mesh batch tests failed'}
