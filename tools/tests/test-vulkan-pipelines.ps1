param(
    [string]$KitRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lcvr-pipelines-' + [Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference = 'Stop'
$state = [IO.File]::ReadAllText((Join-Path $KitRoot 'librw/src/vulkan/vkstate.cpp'))
$header = [IO.File]::ReadAllText((Join-Path $KitRoot 'librw/src/vulkan/rwvk.h'))
$device = [IO.File]::ReadAllText((Join-Path $KitRoot 'librw/src/vulkan/vkdevice.cpp'))
function Extract([string]$Text, [string]$Pattern) {
    $match = [regex]::Match($Text, $Pattern)
    if (-not $match.Success) { throw "Missing production pipeline code: $Pattern" }
    return $match.Value
}
$types = Extract $state '(?ms)^struct PipelineEntry\r?\n\{.*?(?=^struct StateFrame)'
$types += "`n" + (Extract $header '(?ms)^struct FrameBeginStats \{.*?^\};')
$code = Extract $state '(?ms)^uint64\r?\nmakePipelineKey\(.*?^\}'
$code += "`n" + (Extract $state '(?ms)^static void\r?\nwarmStandardPipelines\(.*?^\}')
$code += "`n" + (Extract $state '(?ms)^void\r?\ncompilePendingPipelines\(.*?^\}')
$code += "`n" + (Extract $device '(?m)^void resetFrameBeginStats\(.*$')
$code += "`n" + (Extract $device '(?ms)^bool32 getFrameBeginStats\(.*?^\}')
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'vulkan-pipelines-types.inc'), $types)
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'vulkan-pipelines-production.inc'), $code)
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $KitRoot -OutDir $OutDir -Sources @('test-vulkan-pipelines.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Vulkan pipeline regression failed.' }
