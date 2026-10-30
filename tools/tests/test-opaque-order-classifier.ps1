param([string]$KitRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
      [string]$StageRoot=(Join-Path $KitRoot '.cache/opaque-wrapper-build'),
      [string]$OutDir=(Join-Path $KitRoot '.cache/opaque-wrapper-classifier-tests'))
$ErrorActionPreference='Stop'
$source=[IO.File]::ReadAllText((Join-Path $KitRoot 'librw/src/vulkan/vkpipe.cpp'))
$visibility=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/rw/VisibilityPlugins.cpp'))
function Extract($s,$pattern){$match=[regex]::Match($s,$pattern);if(-not $match.Success){throw 'Production function missing'};return $match.Value}
$wrapper=Extract $visibility '(?ms)^RpAtomic\*\r?\nDefaultRenderCB_pushid\(.*?^\}'
$classifier=Extract $source '(?ms)^bool32\r?\ngetOpaqueOrderInterval\(.*?^\}'
$registration=Extract $visibility '(?ms)#ifndef GTA_PS2\r?\n    // This wrapper.*?#endif'
if($registration -notmatch '\(rw::Atomic::RenderCB\)DefaultRenderCB_pushid' -or
   $registration -notmatch '#else\s+rw::vulkan::setOpaqueOrderDefaultRenderWrapper\(nil\)'){
    throw 'Only audited non-PS2 default wrapper may be registered'
}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'opaque-order-wrapper.inc'),$wrapper)
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'opaque-order-classifier.inc'),$classifier)
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $KitRoot -OutDir $OutDir -Sources @('test-opaque-order-classifier.cpp')
if($LASTEXITCODE -ne 0){throw 'Opaque order production classifier regression failed'}
