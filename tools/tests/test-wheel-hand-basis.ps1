param(
    [Parameter(Mandatory=$true)][string]$StageRoot,
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lcvr-wheel-hand-' + [Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference = 'Stop'
$weapons = [IO.File]::ReadAllText((Join-Path $StageRoot 'src/vr/QuestWeaponVR.cpp'))
$helpers = @()
foreach ($pattern in @(
    '(?ms)^static bool\r?\nBuildLegacyTrackedWeaponHandBasis\(.*?^}',
    '(?ms)^bool\r?\nGetCarWheelHandVisualBasis\(.*?^}'
)) {
    $helper = [regex]::Match($weapons, $pattern)
    if (-not $helper.Success) { throw "Cannot extract hand conversion: $pattern" }
    $helpers += $helper.Value
}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'wheel-hand-raw-production.inc'), ($helpers -join "`n"))
$source = [IO.File]::ReadAllText((Join-Path $StageRoot 'src/core/main.cpp'))
$match = [regex]::Match($source, '(?ms)^static void\r?\nRenderVrTrackedHand\(int hand\).*?(?=\tconst bool holdingDetonator)')
if (-not $match.Success) { throw 'Cannot extract production hand rendering basis.' }
# Capture the final basis immediately before mesh rendering and weapon curls.
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'wheel-hand-basis-production.inc'),
    $match.Value + "`n rendered = {position, right, up, forward};`n}`n")
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-wheel-hand-basis.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Wheel hand basis regression failed.' }
