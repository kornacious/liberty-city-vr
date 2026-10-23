param([string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lc-knockdown-'+[Guid]::NewGuid().ToString('N'))))
$ErrorActionPreference='Stop'
$repo=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$android=[IO.File]::ReadAllText((Join-Path $repo 'overlay/src/skel/android/android.cpp'))
$renderer=[IO.File]::ReadAllText((Join-Path $repo 'librw/src/vulkan/vkdevice.cpp'))
$weapons=[IO.File]::ReadAllText((Join-Path $repo 'overlay/src/vr/QuestWeaponVR.cpp'))
$predicate=[regex]::Match($android,'(?ms)^static bool\r?\nPlayerKnockedDown\(.*?^\}').Value
$anchors=[regex]::Match($renderer,'(?ms)^void\r?\nsetFirstPersonAnchor\(.*?(?=^bool32\r?\ngetFirstPersonViewYaw)').Value
$reset=[regex]::Match($weapons,'(?ms)^static void\r?\nResetInteraction\(\).*?^\}').Value
$knockdownReset=[regex]::Match($weapons,'(?ms)^static void\r?\nResetKnockdownInteraction\(\).*?^\}').Value
$presentation=[regex]::Match($android,'(?ms)^bool\r?\nandroidgame::VrKnockdownPresentationActive\(.*?^\}').Value
$ik=[IO.File]::ReadAllText((Join-Path $repo 'overlay/src/peds/VrBodyIK.cpp'))
$ikContext=[regex]::Match($ik,'(?ms)^static bool\r?\nBodyIKGameplayContext\(.*?^\}').Value
if(-not $predicate -or -not $anchors -or -not $reset -or -not $knockdownReset -or -not $presentation -or -not $ikContext){throw 'Production knockdown/anchor functions not found.'}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'vr-knockdown-production.inc'),(@($predicate,$anchors,$reset,$knockdownReset,$presentation,$ikContext) -join "`n"),[Text.UTF8Encoding]::new($false))
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $repo -OutDir $OutDir -Sources @('test-vr-knockdown.cpp')
if($LASTEXITCODE -ne 0){throw 'Knockdown camera tests failed.'}
