param([string]$OutDir=(Join-Path $PSScriptRoot '../../.cache/hd-texture-tests'))
$ErrorActionPreference='Stop'
$root=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Copy-Item (Join-Path $root 'overlay/src/core/QuestTextureFallback.h') (Join-Path $PSScriptRoot 'texture-fallback-production.inc')
Copy-Item (Join-Path $root 'librw/src/vulkan/VehicleAlphaPolicy.h') (Join-Path $PSScriptRoot 'vehicle-alpha-production.inc')
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -OutDir $OutDir -Sources @('test-hd-texture-loading.cpp')
