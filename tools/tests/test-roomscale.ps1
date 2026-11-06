param([string]$KitRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
 [string]$OutDir=(Join-Path ([IO.Path]::GetTempPath()) ('lcvr-roomscale-'+[Guid]::NewGuid().ToString('N'))))
$ErrorActionPreference='Stop'
$android=[IO.File]::ReadAllText((Join-Path $KitRoot 'overlay/src/skel/android/android.cpp'))
$backend=[IO.File]::ReadAllText((Join-Path $KitRoot 'librw/src/vulkan/vkdevice.cpp'))
function Method([string]$source,[string]$name){
 $value=[regex]::Match($source,'(?ms)^(?:static )?(?:void|bool|bool32)\s+'+[regex]::Escape($name)+'\(.*?^\}').Value
 if(-not $value){throw "Missing production method: $name"};return $value
}
if($android -notmatch 'VrUpdateFirstPersonAnchor\(false\);\s*if\(UpdateQuestRoomscale\(\)\) VrUpdateFirstPersonAnchor\(false\);'){throw 'Roomscale must update gameplay before controller interactions.'}
$fixture=Join-Path $KitRoot ('.cache/roomscale-kit-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
Copy-Item (Join-Path $PSScriptRoot 'test-roomscale.cpp'),(Join-Path $PSScriptRoot 'ragdoll-test-vector.h') $fixture
$methods=@('getFirstPersonPlayBasis','firstPersonWorldVectorToPlay','consumeFirstPersonRoomscale','getFirstPersonViewFrame','setFirstPersonPositionLocked','setFirstPersonHeightLocked','setHeadPose')
$code=($methods | ForEach-Object {Method $backend $_}) -join "`n"
[IO.File]::WriteAllText((Join-Path $fixture 'roomscale-backend.inc'),$code)
[IO.File]::WriteAllText((Join-Path $fixture 'roomscale-player.inc'),((Method $android 'PlayerKnockedDown')+"`n"+(Method $android 'UpdateQuestRoomscale')+"`n"+(Method $android 'androidgame::VrShouldUseTheaterMode')))
[xml]$resources=Get-Content (Join-Path $KitRoot 'tools/manager/EmbeddedTools.resx') -Raw
$helper=@($resources.root.data | Where-Object name -eq 'tools/tests/test-ragdoll.ps1')
[IO.File]::WriteAllText((Join-Path $fixture 'test-ragdoll.ps1'),$helper[0].value)
& (Join-Path $fixture 'test-ragdoll.ps1') -KitRoot $KitRoot -OutDir $OutDir -Sources @('test-roomscale.cpp')
if($LASTEXITCODE -ne 0){throw 'Roomscale regression failed.'}
