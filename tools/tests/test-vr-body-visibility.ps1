param([string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lcvr-body-visibility-' + [Guid]::NewGuid().ToString('N'))))
$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$source = [IO.File]::ReadAllText((Join-Path $repo 'overlay/src/skel/android/android.cpp'))
$policy = [regex]::Match($source, '(?ms)^\tconst bool vehicleBodyIK =.*?^\tgVrHidePlayerBody =.*?;').Value
if (-not $policy) { throw 'Cannot find production body visibility policy.' }
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'vr-body-visibility-production.inc'), $policy)
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $repo -OutDir $OutDir -Sources @('test-vr-body-visibility.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Body visibility tests failed.' }
