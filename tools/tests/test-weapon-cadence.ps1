param(
    [Parameter(Mandatory=$true)][string]$StageRoot,
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lcvr-weapon-cadence-' + [Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference='Stop'
function Extract([string]$relative,[string]$pattern,[string]$output) {
    $source=[IO.File]::ReadAllText((Join-Path $StageRoot $relative))
    $match=[regex]::Match($source,$pattern)
    if (-not $match.Success) { throw "Cannot extract $relative : $pattern" }
    [IO.File]::WriteAllText((Join-Path $PSScriptRoot $output),$match.Value)
    Write-Host ($relative + ' SHA256: ' + (Get-FileHash (Join-Path $StageRoot $relative)).Hash)
}
Extract 'src/peds/PlayerPed.cpp' '(?ms)^\t\tCWeaponInfo \*vrWeaponInfo =.*?(?=^\t// A physical firearm)' 'weapon-cadence-player.inc'
Extract 'src/vehicles/Automobile.cpp' '(?ms)^\t\tconst uint32 now = CTimer::GetTimeInMilliseconds\(\);\r?\n\t\tfor\(int hand = 0; hand < 2; hand\+\+\).*?(?=^\t\t// A tracked weapon)' 'weapon-cadence-car.inc'
Extract 'src/weapons/Weapon.cpp' '(?ms)^\t\t\tif \(m_nAmmoInClip > 0\).*?CStats::RoundsFiredByPlayer\+\+;' 'weapon-cadence-ammo.inc'
Extract 'src/weapons/Weapon.cpp' '(?ms)^void\r?\nCWeapon::Reload\(void\).*?(?=^void\r?\nFireOneInstantHitRound)' 'weapon-cadence-update.inc'
Copy-Item (Join-Path $StageRoot 'src/weapons/WeaponType.h') (Join-Path $PSScriptRoot 'weapon-cadence-types.inc')
Copy-Item (Join-Path $StageRoot 'src/vr/QuestWeaponTiming.h') (Join-Path $PSScriptRoot 'weapon-cadence-clock.inc')
Extract 'src/vr/QuestWeaponVR.cpp' '(?ms)^int\r?\nGetQuestWeaponSettingCount\(\).*?(?=^int\r?\nGetQuestCalibrationWeaponType)' 'pistol-fire-settings.inc'
Extract 'src/vr/QuestWeaponVR.cpp' '(?ms)^\tgPistolHoldToRepeat = GetPrivateProfileIntA\(.*?;' 'pistol-fire-load.inc'
Extract 'src/skel/android/vrdebug.cpp' '(?ms)^enum eVrWeaponsMenuItem \{.*?^};' 'pistol-fire-menu-enum.inc'
Extract 'src/skel/android/vrdebug.cpp' '(?ms)^static int\r?\nQuestWeaponSettingForWeaponItem\(.*?^}' 'pistol-fire-menu-map.inc'
Extract 'src/skel/android/vrdebug.cpp' '(?ms)^static void\r?\nDrawQuestWeaponsPage\(.*?^}' 'pistol-fire-menu-draw.inc'
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-weapon-cadence.cpp','test-pistol-fire-settings.cpp')
