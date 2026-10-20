param(
    [string]$KitRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$OutDir = (Join-Path $KitRoot '.cache/physical-scope-tests')
)
$ErrorActionPreference = 'Stop'
$device = [IO.File]::ReadAllText((Join-Path $KitRoot 'librw/src/vulkan/vkdevice.cpp'))
$debug = [IO.File]::ReadAllText((Join-Path $KitRoot 'overlay/src/skel/android/vrdebug.cpp'))
function Extract([string]$source, [string]$pattern) {
    $match = [regex]::Match($source, $pattern)
    if (-not $match.Success) { throw "Cannot extract production scope code: $pattern" }
    return $match.Value
}
$code = Extract $device '(?ms)^struct ScopeDraw \{.*?^\};'
$code += "`nstatic std::vector<ScopeDraw> gScopeWorldDraws, gScopeLensDraws;`n"
$code += Extract $device '(?ms)^bool32 capturePhysicalScopeDraw\(.*?^\}'
$code += "`n" + (Extract $device '(?ms)^static void\r?\nreplayScopeDraws\(.*?^\}')
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'physical-scope-production.inc'), $code)
$zoom = Extract $debug '(?ms)^float\r?\nVrScopeZoomFactor\(void\).*?^\}'
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'physical-scope-zoom.inc'), $zoom)
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $KitRoot -OutDir $OutDir -Sources @('test-physical-scope.cpp')
