param([string]$KitRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference='Stop'
$fixture=Join-Path $KitRoot ('.cache/jumpsuit-hands-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$source=Get-Content (Join-Path $KitRoot 'overlay/src/peds/VrBodyIK.cpp') -Raw
$policy=[regex]::Match($source,'(?ms)^bool\r?\nUsesOutfitHands\(void\)\r?\n\{.*?^\}').Value
if(-not $policy){throw 'Missing production outfit hand policy.'}
[IO.File]::WriteAllText((Join-Path $fixture 'jumpsuit-hands-production.inc'),$policy)
$weapons=Get-Content (Join-Path $KitRoot 'overlay/src/vr/QuestWeaponVR.cpp') -Raw
$holsters=[regex]::Match($weapons,'(?ms)^bool\r?\nGetWeaponHolsterMatrix\(int slot, CMatrix \*matrix, int copy\)\r?\n\{.*?^\}').Value
if(-not $holsters){throw 'Missing production holster rendering policy.'}
[IO.File]::WriteAllText((Join-Path $fixture 'vehicle-holsters-production.inc'),$holsters)
Copy-Item (Join-Path $PSScriptRoot 'test-jumpsuit-hands.cpp') $fixture
[xml]$resources=Get-Content (Join-Path $KitRoot 'tools/manager/EmbeddedTools.resx') -Raw
$runner=($resources.root.data | Where-Object name -eq 'tools/tests/test-ragdoll.ps1').value
[IO.File]::WriteAllText((Join-Path $fixture 'test-ragdoll.ps1'),$runner)
& (Join-Path $fixture 'test-ragdoll.ps1') -KitRoot $KitRoot -OutDir (Join-Path $fixture 'out') -Sources @('test-jumpsuit-hands.cpp')
