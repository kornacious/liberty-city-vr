param([string]$OutDir)
$ErrorActionPreference='Stop'
$kit=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$source=[IO.File]::ReadAllText((Join-Path $kit 'librw/src/vulkan/vkpipe.cpp'))
$match=[regex]::Match($source,'(?m)^selectAtomicAlphaPasses\(')
if(-not $match.Success){throw 'Missing production uniform-alpha selector'}
$brace=$source.IndexOf('{',$match.Index);$depth=1;$end=$brace+1
while($depth-gt0-and$end-lt$source.Length){if($source[$end]-eq'{'){$depth++};if($source[$end]-eq'}'){$depth--};$end++}
if($depth-ne0){throw 'Unclosed production uniform-alpha selector'}
$raster=[IO.File]::ReadAllText((Join-Path $kit 'librw/src/vulkan/vkraster.cpp'))
$bounded=[regex]::Match($raster,'(?ms)^bool32\r?\nrasterHasNormalizedAlpha\(.*?^\}')
if(-not $bounded.Success){throw 'Missing production normalized alpha texture proof'}
$setter=[regex]::Match($source,'(?m)^void setMaskedAlphaPassSkipEnabled\([^\r\n]+').Value
$getter=[regex]::Match($source,'(?m)^bool32 getMaskedAlphaPassSkipEnabled\([^\r\n]+').Value
if(-not $setter -or -not $getter){throw 'Masked-alpha toggle API missing'}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'uniform-alpha-production.inc'),
    ($setter+"`n"+$getter+"`n"+$bounded.Value+"`nstatic uint32`n"+$source.Substring($match.Index,$end-$match.Index)),[Text.UTF8Encoding]::new($false))
$match=[regex]::Match($source,'(?m)^useOpaqueWorldPipeline\(')
if(-not $match.Success){throw 'Missing production opaque-world selector'}
$brace=$source.IndexOf('{',$match.Index);$depth=1;$end=$brace+1
while($depth-gt0-and$end-lt$source.Length){if($source[$end]-eq'{'){$depth++};if($source[$end]-eq'}'){$depth--};$end++}
[IO.File]::AppendAllText((Join-Path $PSScriptRoot 'uniform-alpha-production.inc'),
    ("`nstatic bool`n"+$source.Substring($match.Index,$end-$match.Index)),[Text.UTF8Encoding]::new($false))
# Exercise the actual lookup/state-restoration block, including failure returns.
$pipeline=[regex]::Match($source,'(?s)const uint32 savedAlphaFunction = gstate.alphaTestFunction;.*?gstate.alphaTestFunction = savedAlphaFunction;')
if(-not $pipeline.Success){throw 'Missing atomic pipeline state block'}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'opaque-world-pipeline.inc'),$pipeline.Value,[Text.UTF8Encoding]::new($false))
$shader=[IO.File]::ReadAllText((Join-Path $kit 'librw/src/vulkan/shaders/rw_world.frag'))
foreach($rule in @('if(colour.a < reference)','else if(colour.a >= -reference)')){
    if(-not $shader.Contains($rule)){throw 'Shader alpha rules changed; update the differential coverage test'}
}
$skip=$source.IndexOf('if(!(alphaPasses&(1u<<gsPass)))continue;')
$state=$source.IndexOf('if((vehicleAlphaPass && wantsBlend && !vehicleSplit)')
if($skip-lt0-or$skip-ge$state){throw 'Empty-pass rejection must precede render-state changes'}
if(-not $source.Contains('(gsPass==firstGsPass?DrawProfile::Mesh:DrawProfile::Extra)')){
    throw 'The surviving faint half must count as the primary mesh pass'
}
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -OutDir $OutDir -Sources @('test-uniform-alpha.cpp')
if($LASTEXITCODE-ne0){throw 'Uniform alpha regression failed'}
