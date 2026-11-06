param(
    [string]$KitRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$OutDir=(Join-Path ([IO.Path]::GetTempPath()) ('lcvr-posture-'+[Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference='Stop'
$source=[IO.File]::ReadAllText((Join-Path $KitRoot 'overlay/src/peds/VrBodyIK.cpp'))
$runtime=[regex]::Match($source,'(?ms)^static void MovePostureJoint\(.*?(?=^static void ApplySkinned\()').Value
if(-not $runtime){throw 'Missing production posture adapter.'}
if($source -notmatch '(?s)ApplyTrackedPosture\(ped,hier,mats,upperBody,visualBody,authoredDrop\);.*?CMatrix desiredWorld\[2\]'){
    throw 'Posture must be applied before tracked arm targets are solved.'
}
$fixture=Join-Path $KitRoot ('.cache/posture-kit-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
Copy-Item (Join-Path $PSScriptRoot 'test-body-posture.cpp'),(Join-Path $PSScriptRoot 'ragdoll-test-vector.h') $fixture
[IO.File]::WriteAllText((Join-Path $fixture 'body-posture-runtime.inc'),$runtime)
[xml]$resources=Get-Content (Join-Path $KitRoot 'tools/manager/EmbeddedTools.resx') -Raw
$helper=@($resources.root.data | Where-Object name -eq 'tools/tests/test-ragdoll.ps1')
if($helper.Count -ne 1){throw 'Missing embedded native compiler helper.'}
[IO.File]::WriteAllText((Join-Path $fixture 'test-ragdoll.ps1'),$helper[0].value)
& (Join-Path $fixture 'test-ragdoll.ps1') -KitRoot $KitRoot -OutDir $OutDir -Sources @('test-body-posture.cpp')
if($LASTEXITCODE -ne 0){throw 'Body posture regression failed.'}
