param([string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lc-crouch-view-'+[Guid]::NewGuid().ToString('N'))))
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$anchor = [IO.File]::ReadAllText((Join-Path $repo 'overlay/src/skel/android/android.cpp'))
if($anchor -notmatch 'followCrouch = knockdownContext &&\s+OculusVR::AreTrackedHandsEnabled\(\) &&' -or
   $anchor -notmatch 'if\(player->bIsDucking && !OculusVR::AreTrackedHandsEnabled\(\)\)'){
    throw 'Visible hands must follow the animated head; hidden hands must use the fixed crouch anchor.'
}
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $repo -OutDir $OutDir -Sources @('test-vr-crouch-view.cpp')
if($LASTEXITCODE -ne 0){throw 'Crouch camera tests failed.'}
