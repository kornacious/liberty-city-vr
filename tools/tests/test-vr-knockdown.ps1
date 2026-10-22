param([string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lc-knockdown-'+[Guid]::NewGuid().ToString('N'))))
$ErrorActionPreference='Stop'
$repo=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$android=[IO.File]::ReadAllText((Join-Path $repo 'overlay/src/skel/android/android.cpp'))
$renderer=[IO.File]::ReadAllText((Join-Path $repo 'librw/src/vulkan/vkdevice.cpp'))
$predicate=[regex]::Match($android,'(?ms)^static bool\r?\nPlayerKnockedDown\(.*?^\}').Value
$anchors=[regex]::Match($renderer,'(?ms)^void\r?\nsetFirstPersonAnchor\(.*?(?=^bool32\r?\ngetFirstPersonViewYaw)').Value
if(-not $predicate -or -not $anchors){throw 'Production knockdown/anchor functions not found.'}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'vr-knockdown-production.inc'),$predicate+"`n"+$anchors,[Text.UTF8Encoding]::new($false))
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $repo -OutDir $OutDir -Sources @('test-vr-knockdown.cpp')
if($LASTEXITCODE -ne 0){throw 'Knockdown camera tests failed.'}
