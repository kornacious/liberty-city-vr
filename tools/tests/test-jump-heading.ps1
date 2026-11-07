param([Parameter(Mandatory=$true)][string]$StageRoot,
 [string]$KitRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
 [string]$OutDir=(Join-Path ([IO.Path]::GetTempPath()) ('lcvr-jump-heading-'+[Guid]::NewGuid().ToString('N'))))
$ErrorActionPreference='Stop'
$source=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/peds/Ped.cpp'))
$fixture=Join-Path $KitRoot ('.cache/jump-heading-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
Copy-Item (Join-Path $PSScriptRoot 'test-jump-heading.cpp') $fixture
$heading=[regex]::Match($source,'(?ms)^static bool\r?\nUpdateQuestJumpHeading\(.*?^}').Value
$walk=[regex]::Match($source,'(?ms)^float\r?\nCPed::WorkOutHeadingForMovingFirstPerson\(.*?^}').Value
$launch=$source.Substring($source.IndexOf('CPed::FinishLaunchCB(CAnimBlendAssociation'))
$start=$launch.IndexOf('if(questJump){');$end=$launch.IndexOf('#ifdef VC_PED_PORTS',$start)
if(-not $heading -or -not $walk -or $start -lt 0 -or $end -lt 0){throw 'Missing production jump blocks.'}
if($source -notmatch '(?s)CPed::SetJump\(void\).*?UpdateQuestJumpHeading\(this\);.*?SetStoredState\(\);'){throw 'Jump request must refresh heading.'}
if($launch -notmatch '(?s)UpdateQuestJumpHeading\(ped\);\s+CVector forward\(0.15f.*?CWorld::TestSphereAgainstWorld'){throw 'Takeoff must refresh heading before collision probes.'}
[IO.File]::WriteAllText((Join-Path $fixture 'jump-heading.inc'),$heading)
[IO.File]::WriteAllText((Join-Path $fixture 'jump-walk.inc'),$walk)
[IO.File]::WriteAllText((Join-Path $fixture 'jump-impulse.inc'),$launch.Substring($start,$end-$start))
[xml]$resources=Get-Content (Join-Path $KitRoot 'tools/manager/EmbeddedTools.resx') -Raw
$helper=$resources.root.data | Where-Object name -eq 'tools/tests/test-ragdoll.ps1'
$runner=([string]$helper.value).Replace('/nologo /EHsc','/nologo /DRW_VULKAN /D__ANDROID__ /EHsc')
[IO.File]::WriteAllText((Join-Path $fixture 'test-ragdoll.ps1'),$runner)
& (Join-Path $fixture 'test-ragdoll.ps1') -KitRoot $KitRoot -OutDir $OutDir -Sources @('test-jump-heading.cpp')
if($LASTEXITCODE -ne 0){throw 'Physical jump regression failed.'}
