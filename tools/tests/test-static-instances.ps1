param([string]$OutDir=(Join-Path $PSScriptRoot '../../.cache/static-instance-flush-tests'))
$ErrorActionPreference='Stop'
$root=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$source=[IO.File]::ReadAllText((Join-Path $root 'librw/src/vulkan/vkpipe.cpp'))
foreach($part in @(@('draw','(?ms)^struct StaticInstanceDraw \{.*?^\};'),@('flush','(?ms)^void flushStaticInstances\(void\)\{.*?^\}'),@('bounds','(?ms)^static StaticInstances::Bounds staticInstanceBounds\(.*?^\}'))){
    $match=[regex]::Match($source,$part[1])
    if(-not $match.Success){throw ('Missing production '+$part[0])}
    [IO.File]::WriteAllText((Join-Path $PSScriptRoot ('static-instance-'+$part[0]+'-production.inc')),$match.Value)
}
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -OutDir $OutDir -Sources @('test-static-instances.cpp','test-static-instance-flush.cpp','test-perf-capture.cpp')
if($LASTEXITCODE -ne 0){throw 'Static instance regression failed'}
