param([string]$OutDir)
$ErrorActionPreference='Stop'
$kit=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$source=[IO.File]::ReadAllText((Join-Path $kit 'librw/src/vulkan/vkdevice.cpp'))
function Definition([string]$name){
    $match=[regex]::Match($source,'(?m)^(?:(?:static )?(?:void|bool32)\r?\n)?(?:(?:static )?(?:void|bool32) )?'+$name+'\(')
    if(-not$match.Success){throw "Missing GPU backend function: $name"}
    $brace=$source.IndexOf('{',$match.Index);$depth=1;$end=$brace+1
    while($depth-gt0-and$end-lt$source.Length){if($source[$end]-eq'{'){$depth++};if($source[$end]-eq'}'){$depth--};$end++}
    if($depth-ne0){throw "Unclosed GPU backend function: $name"}
    return $source.Substring($match.Index,$end-$match.Index)
}
$pieces=@()
foreach($name in @('setGpuProfileFrame','writeGpuPassMarker','gpuPassRecording','beginGpuProfilePass','endGpuProfilePass','countGpuProfileDraw','getGpuPassProfile','readCompletedFrameTimestamps','setGpuFrameTimingEnabled','getGpuFrameTimeMs')){$pieces+=(Definition $name)}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'gpu-pass-backend.inc'),($pieces -join "`n"),[Text.UTF8Encoding]::new($false))
foreach($pattern in @('queryInfo.queryCount = GpuPassProfile::QueryCount','frame.gpuProfile.sampled?GpuPassProfile::QueryCount:2','frame.timestampPending = frame.gpuTimingActive','beginGpuProfilePass(GpuPassProfile::Wrist)','beginGpuProfilePass(GpuPassProfile::Scope)','beginGpuProfilePass(GpuPassProfile::Postprocess)','beginGpuProfilePass(GpuPassProfile::Scene)','endGpuProfilePass(GpuPassProfile::Scene)')){
    if(-not$source.Contains($pattern)){throw "Missing GPU integration: $pattern"}
}
if($source -notmatch 'beginGpuProfilePass\(GpuPassProfile::Scene\);\s*vkCmdBeginRenderPass' -or
   $source -notmatch 'vkCmdEndRenderPass\(gvk.frameCommands\);\s*endGpuProfilePass\(GpuPassProfile::Scene\);\s*beginGpuProfilePass\(GpuPassProfile::Postprocess\)'){
    throw 'Whole-scene GPU timestamps must bracket the native render pass from outside'
}
foreach($file in @('vkpipe.cpp','vkim.cpp')){
    $draw=[IO.File]::ReadAllText((Join-Path $kit ('librw/src/vulkan/'+$file)))
    if(-not$draw.Contains('countGpuProfileDraw();')){throw "Missing actual draw accounting: $file"}
}
if((Definition 'readCompletedFrameTimestamps')-match'VK_QUERY_RESULT_WAIT_BIT'){throw 'GPU profiler must never wait for queries'}
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -OutDir $OutDir -Sources @('test-gpu-pass-profile.cpp')
if($LASTEXITCODE-ne0){throw 'GPU pass profiling regression failed'}
