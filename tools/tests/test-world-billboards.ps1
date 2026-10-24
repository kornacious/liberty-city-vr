param(
    [Parameter(Mandatory=$true)][string]$StageRoot,
    [string]$OutDir = (Join-Path $env:TEMP ('lcvr-billboards-' + [guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference='Stop'
$kitRoot=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$kitHeader=Join-Path $kitRoot 'overlay/src/renderer/QuestBillboard.h'
$stageHeader=Join-Path $StageRoot 'src/renderer/QuestBillboard.h'
if((Get-FileHash $kitHeader).Hash -ne (Get-FileHash $stageHeader).Hash){
    throw 'Assembled billboard helper differs from the production header under test.'
}
$sprite=Get-Content -Raw (Join-Path $StageRoot 'src/renderer/Sprite.cpp')
$world=$sprite.Substring(0,$sprite.IndexOf('CSprite::Set6Vertices2D'))
if($world -notmatch 'window->x, window->y, worldUp, worldEast' -or
   $world -notmatch 'worldUp\[3\] = \{ -camera->right.z, camera->up.z, camera->at.z \}'){
    throw 'Billboards must receive world axes expressed in the actual render camera frame.'
}
if(([regex]::Matches($world,'FaceQuestWorldSprite\((?:vert|verts), [46], x, y\);')).Count -ne 6){
    throw 'All six world sprite routines must orient the complete quad.'
}
if(([regex]::Matches($world,'if\(!QuestWorldBillboardsEnabled\(\)\)')).Count -ne 6){
    throw 'Legacy mono clipping must be bypassed before stereo reorientation.'
}
if($sprite.Substring($sprite.IndexOf('CSprite::Set6Vertices2D')) -match 'FaceQuestWorldSprite'){
    throw 'Screen-space sprites must retain their original geometry.'
}
if($world -notmatch 'return !androidgame::VrShouldUseTheaterMode\(\);'){
    throw 'The mono theater path must retain its existing projection.'
}
$shader=Get-Content -Raw (Join-Path $StageRoot 'vendor/librw/src/vulkan/shaders/rw_im2d.vert')
$clouds=Get-Content -Raw (Join-Path $StageRoot 'src/renderer/Clouds.cpp')
if($clouds -notmatch '(?s)if\(!androidgame::VrShouldUseTheaterMode\(\)\)\{\s*CClouds::ms_cameraRoll = 0.0f;\s*return;'){
    throw 'Clouds must not apply the old camera-roll correction over the world-oriented billboard.'
}
if($shader -notmatch 'worldBillboard \? inPosition.w : max\(inPosition.w, panel\)'){
    throw 'Near world sprites must not be clamped to the HUD plane.'
}
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -OutDir $OutDir -Sources @('test-world-billboards.cpp')
if($LASTEXITCODE -ne 0){ throw 'Billboard geometry tests failed.' }
