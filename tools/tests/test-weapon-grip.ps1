param(
    [string]$StageRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$OutDir = (Join-Path $StageRoot '.cache/weapon-grip-tests')
)
$ErrorActionPreference = 'Stop'
$sourcePath = Join-Path $StageRoot 'src/vr/QuestWeaponVR.cpp'
if (-not (Test-Path $sourcePath)) { $sourcePath = Join-Path $StageRoot 'overlay/src/vr/QuestWeaponVR.cpp' }
$source = [IO.File]::ReadAllText($sourcePath)
function Extract([string]$pattern) {
    $m = [regex]::Match($source, $pattern)
    if (-not $m.Success) { throw "Cannot extract production grip code: $pattern" }
    return $m.Value
}
$code = Extract '(?ms)^enum \{\r?\n\tVR_HAND_COUNT.*?(?=^struct DroppedWeapon)'
$code += Extract '(?ms)^static void\r?\nApplyBuiltInHandCalibration\(.*?(?=^static void\r?\nGetDefaultCalibration\()'
$code += Extract '(?ms)^static bool\r?\nBuildWeaponModelMatrixBase\(int hand,.*?^}'
$code += Extract '(?ms)^static void\r?\nGetSupportModelAxes\(.*?^}'
$code += Extract '(?ms)^static CVector\r?\nGetWeaponLocalBarrelDirection\(.*?^}'
$code += Extract '(?ms)^static CVector\r?\nGetWeaponLocalMuzzle\(.*?^}'
$code += Extract '(?ms)^static bool\r?\nBuildWeaponMuzzleRay\(.*?^}'
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'weapon-grip-production.inc'), $code)
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'weapon-calibration-production.inc'),
    (Extract '(?ms)^static int \*\r?\nQuestCalibrationValue\(.*?(?=^int\r?\nGetQuestHolsterPointCount\()'))
$types = Join-Path $StageRoot 'src/weapons/WeaponType.h'
if (-not (Test-Path $types)) { throw 'Pass -StageRoot pointing to an assembled Liberty City source tree.' }
Copy-Item $types (Join-Path $PSScriptRoot 'weapon-grip-types.inc')
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-weapon-grip.cpp')
