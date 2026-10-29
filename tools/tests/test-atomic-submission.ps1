param([string]$OutDir)
$ErrorActionPreference='Stop'
$kit=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$source=[IO.File]::ReadAllText((Join-Path $kit 'librw/src/vulkan/vkpipe.cpp'))
$start=$source.IndexOf('if(bindings.PipelineBind(pipeline))')
$end=$source.IndexOf('if(material && (geometry->flags & Geometry::MODULATE))',$start)
if($start-lt0-or$end-le$start){throw 'Missing production atomic binding block'}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'atomic-submission-production.inc'),
    $source.Substring($start,$end-$start),[Text.UTF8Encoding]::new($false))
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -OutDir $OutDir -Sources @('test-atomic-submission.cpp')
if($LASTEXITCODE-ne0){throw 'Atomic submission equivalence regression failed'}
