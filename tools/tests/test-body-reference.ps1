param([string]$KitRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
 [string]$OutDir=(Join-Path ([IO.Path]::GetTempPath()) ('lcvr-body-reference-'+[Guid]::NewGuid().ToString('N'))))
$ErrorActionPreference='Stop'
$source=[IO.File]::ReadAllText((Join-Path $KitRoot 'overlay/src/peds/VrBodyIK.cpp'))
$fixture=Join-Path $KitRoot ('.cache/body-reference-kit-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
Copy-Item (Join-Path $PSScriptRoot 'test-body-reference.cpp'),(Join-Path $PSScriptRoot 'ragdoll-test-vector.h') $fixture
$types=@('ArmBones','ShoulderAnchor','UpperBodyBones','UpperBodyAnchor') | ForEach-Object {
 $value=[regex]::Match($source,'(?ms)^struct '+$_+'\s*\{.*?^};').Value
 if(-not $value){throw "Missing type $_"};$value
}
[IO.File]::WriteAllText((Join-Path $fixture 'body-reference-types.inc'),($types -join "`n"))
$methods=@('ClampF','Unit','Orthonormalise','ToBasisLocal','FromBasisLocal','BuildCrouchPoseBasis','AlignCrouchAxes','AlignGroundedSkeleton','BuildGroundedVisualBody','StabilizeUpperBody','StabilizeShoulderRoot') | ForEach-Object {
 $value=[regex]::Match($source,'(?ms)^static (?:float|CVector|void|bool) '+$_+'\(.*?^}').Value
 if(-not $value){throw "Missing production method $_"};$value
}
[IO.File]::WriteAllText((Join-Path $fixture 'body-reference-production.inc'),($methods -join "`n"))
[xml]$resources=Get-Content (Join-Path $KitRoot 'tools/manager/EmbeddedTools.resx') -Raw
$helper=@($resources.root.data | Where-Object name -eq 'tools/tests/test-ragdoll.ps1')
[IO.File]::WriteAllText((Join-Path $fixture 'test-ragdoll.ps1'),$helper[0].value)
& (Join-Path $fixture 'test-ragdoll.ps1') -KitRoot $KitRoot -OutDir $OutDir -Sources @('test-body-reference.cpp')
if($LASTEXITCODE -ne 0){throw 'Body reference regression failed.'}
