param(
    [string]$KitRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lcvr-descriptors-' + [Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference = 'Stop'
$vkRoot = Join-Path $KitRoot 'librw/src/vulkan'
$state = [IO.File]::ReadAllText((Join-Path $vkRoot 'vkstate.cpp'))
$device = [IO.File]::ReadAllText((Join-Path $vkRoot 'vkdevice.cpp'))
$raster = [IO.File]::ReadAllText((Join-Path $vkRoot 'vkraster.cpp'))
$header = [IO.File]::ReadAllText((Join-Path $vkRoot 'rwvkimpl.h'))
function Extract([string]$Source, [string]$Pattern) {
    $match = [regex]::Match($Source, $Pattern)
    if (-not $match.Success) { throw "Cannot extract production descriptor code: $Pattern" }
    return $match.Value
}
$types = Extract $header '(?ms)^struct VulkanRaster\r?\n\{.*?^\};'
$types += "`n" + (Extract $state '(?ms)^struct TextureDescriptorEntry\r?\n\{.*?(?=^namespace \{)')
$code = Extract $state '(?ms)^uint32\r?\nmakeSamplerKey\(.*?^\}'
$code += "`n" + (Extract $state '(?ms)^void\r?\nfreeTextureDescriptorSets\(.*?^\}')
$code += "`n" + (Extract $device '(?ms)^struct RetiredBuffer\r?\n.*?(?=^// -+\r?\n// Shared helpers)')
$code += "`n" + (Extract $state '(?ms)^void\r?\nretireRasterTextureDescriptors\(.*?^\}')
$code += "`n" + (Extract $state '(?ms)^VkDescriptorSet\r?\ngetTextureDescriptor\(.*?^\}')
$code += "`n" + (Extract $raster '(?ms)^static void\*\r?\ndestroyNativeRaster\(.*?^\}')
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'vulkan-descriptors-types.inc'), $types)
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'vulkan-descriptors-production.inc'), $code)
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $KitRoot -OutDir $OutDir -Sources @('test-vulkan-descriptors.cpp')
