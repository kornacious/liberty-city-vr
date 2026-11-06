param(
 [string]$KitRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
 [string]$OutDir=(Join-Path ([IO.Path]::GetTempPath()) ('lcvr-head-fit-'+[Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference='Stop'
$source=[IO.File]::ReadAllText((Join-Path $KitRoot 'overlay/src/skel/android/vrdebug.cpp'))
$persistence=[regex]::Match($source,'(?ms)^static void\r?\nLoadXboxHeadFit\(\).*?(?=^static int\r?\nVrControlsRowSource)').Value
$api=[regex]::Match($source,'(?ms)^void VrGetXboxHeadOffset\(.*?^bool VrXboxKnockdownBoneViewEnabled\(\).*?$').Value
$action=[regex]::Match($source,'(?ms)else if\(gVrMenuPage == VR_MENU_PAGE_HEAD_CALIBRATION &&\s*\(positivePulse \|\| decreasePulse\)\)\{(.*?)(?=^\t\t\}else if\(gVrMenuPage == VR_MENU_PAGE_CONTROLS)').Groups[1].Value
if(-not $persistence -or -not $api -or -not $action){throw 'Missing production fit persistence, getters or menu actions.'}
$fixture=Join-Path $KitRoot ('.cache/head-fit-kit-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
Copy-Item (Join-Path $PSScriptRoot 'test-head-calibration.cpp'),(Join-Path $PSScriptRoot 'ragdoll-test-vector.h') $fixture
[IO.File]::WriteAllText((Join-Path $fixture 'head-calibration-persistence.inc'),$persistence)
[IO.File]::WriteAllText((Join-Path $fixture 'head-calibration-api.inc'),$api)
[IO.File]::WriteAllText((Join-Path $fixture 'head-calibration-action.inc'),$action)
[xml]$resources=Get-Content (Join-Path $KitRoot 'tools/manager/EmbeddedTools.resx') -Raw
$helper=@($resources.root.data | Where-Object name -eq 'tools/tests/test-ragdoll.ps1')
if($helper.Count -ne 1){throw 'Missing embedded native compiler helper.'}
[IO.File]::WriteAllText((Join-Path $fixture 'test-ragdoll.ps1'),$helper[0].value)
& (Join-Path $fixture 'test-ragdoll.ps1') -KitRoot $KitRoot -OutDir $OutDir -Sources @('test-head-calibration.cpp')
if($LASTEXITCODE -ne 0){throw 'Head calibration regression failed.'}
