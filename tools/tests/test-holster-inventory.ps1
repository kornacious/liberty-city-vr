param(
    [Parameter(Mandatory=$true)][string]$StageRoot,
    [string]$OutDir = (Join-Path $env:TEMP ('lc-holster-' + [guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference = 'Stop'
$StageRoot = [IO.Path]::GetFullPath($StageRoot)
$pad = [IO.File]::ReadAllText((Join-Path $StageRoot 'src/core/Pad.cpp'))
$quest = [IO.File]::ReadAllText((Join-Path $StageRoot 'src/vr/QuestWeaponVR.cpp'))
$parts = @('#include "' + ((Join-Path $StageRoot 'src/weapons/WeaponType.h') -replace '\\','/') + '"')
foreach ($name in @('GetWeaponTypeForSlot', 'GetVrHolsterSlotName', 'GetVrWeaponTypeForSlot', 'IsVrWeaponSlotOwned', 'WeaponCheat')) {
    $match = [regex]::Match($pad, '(?ms)^(?:static )?[^\r\n]+[ *]' + $name + '\([^\r\n]*\)\r?\n\{.*?^\}')
    if (-not $match.Success) { throw "Missing production function $name" }
    $parts += $match.Value
}
foreach ($pattern in @(
    '(?ms)^static bool\r?\nIsPhysicalGun\(.*?^\}',
    '(?ms)^static bool\r?\nIsPhysicalMelee\(.*?^\}',
    '(?ms)^static bool\r?\nIsPhysicalThrowable\(.*?^\}',
    '(?m)^bool IsPhysicalWeaponType[^\r\n]+',
    '(?ms)^void\r?\nCycleQuestHolsterPointSlot\(.*?^\}'
)) {
    $match = [regex]::Match($quest, $pattern)
    if (-not $match.Success) { throw "Missing production code $pattern" }
    $parts += $match.Value
}
$generated = Join-Path $PSScriptRoot 'holster-inventory-production.inc'
[IO.File]::WriteAllText($generated, ($parts -join "`n`n"), [Text.UTF8Encoding]::new($false))
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-holster-inventory.cpp')
