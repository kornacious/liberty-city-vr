param(
    [Parameter(Mandatory=$true)][string]$StageRoot,
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lcvr-roads-' + [Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference = 'Stop'
$renderer = [IO.File]::ReadAllText((Join-Path $StageRoot 'src/renderer/Renderer.cpp'))
$backend = [IO.File]::ReadAllText((Join-Path $StageRoot 'vendor/librw/src/vulkan/vkdevice.cpp'))
function Definition([string]$source, [string]$name, [string]$returnType) {
    $match = [regex]::Match($source, '(?m)^' + [regex]::Escape($name) + '\(')
    if (-not $match.Success) { throw "Missing production function: $name" }
    $brace = $source.IndexOf('{', $match.Index)
    $depth = 1; $end = $brace + 1
    while ($depth -gt 0 -and $end -lt $source.Length) {
        if ($source[$end] -eq '{') { $depth++ }
        if ($source[$end] -eq '}') { $depth-- }
        $end++
    }
    if ($depth -ne 0) { throw "Unclosed production function: $name" }
    return $returnType + "`n" + $source.Substring($match.Index, $end - $match.Index)
}
$parts = @('#ifdef ROAD_CULLING_BACKEND')
foreach ($name in @('sphereIntersectsClipSidePlanes', 'isFirstPersonWorldSphereVisibleInStereo', 'isFirstPersonWorldBoxVisibleInStereo')) {
    $parts += Definition $backend $name 'bool32'
}
$parts += '#else'
$parts += '#ifdef ROAD_CULLING_HELPER'
$parts += Definition $renderer 'CRenderer::ShouldRenderInCurrentView' 'bool'
$parts += '#endif'
$parts += Definition $renderer 'CRenderer::RenderOneRoad' 'void'
$parts += '#endif'
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'road-culling-production.inc'), ($parts -join "`n"), [Text.UTF8Encoding]::new($false))
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -OutDir $OutDir -Sources @('test-road-culling.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Road visibility regression failed.' }
