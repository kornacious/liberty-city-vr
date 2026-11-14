param(
 [string]$KitRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
 [string]$OutDir=(Join-Path ([IO.Path]::GetTempPath()) ('lcvr-train-transitions-'+[Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference='Stop'
$fixture=Join-Path $KitRoot ('.cache/train-transitions-kit-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
Copy-Item (Join-Path $PSScriptRoot 'test-vehicle-seat-recenter.cpp'),(Join-Path $PSScriptRoot 'test-head-attachment.cpp'),(Join-Path $PSScriptRoot 'ragdoll-test-vector.h') $fixture
[xml]$resources=Get-Content (Join-Path $KitRoot 'tools/manager/EmbeddedTools.resx') -Raw
foreach($name in @('test-ragdoll.ps1','test-vehicle-seat-recenter.ps1','test-head-attachment.ps1')){
 $resource=@($resources.root.data | Where-Object name -eq ('tools/tests/'+$name))
 if($resource.Count -ne 1){throw "Missing embedded native test runner: $name"}
 $runner=$resource[0].value
 if($name -eq 'test-vehicle-seat-recenter.ps1'){
  # Exercise the actual hand visibility predicate rather than a stub, alongside
  # the production train policy, native exit handoff and renderer tracking code.
  $marker='$handPolicy = [regex]::Match'
  if(-not $runner.Contains($marker)){throw 'Seat runner hand-policy seam changed.'}
  $extra=@'
$handEnabled = [regex]::Match($weapons, '(?m)^bool AreTrackedHandsEnabled\(\).*').Value
if(-not $handEnabled){throw 'Missing production tracked-hands predicate.'}
$code += 'namespace OculusVR {' + "`n" + $handEnabled + "`n}`n"
'@
  $runner=$runner.Replace($marker,$extra+"`n"+$marker)
 }
 [IO.File]::WriteAllText((Join-Path $fixture $name),$runner)
}
$patch=[IO.File]::ReadAllText((Join-Path $KitRoot 'patches/01-re3-liberty-vr.patch'))
if($patch -notmatch '(?s)ped->SetPosition\(posAfterExit\).*?veh->RemovePassenger\(ped\).*?OculusVR::FinishQuestXboxTrainExit\(ped, veh\)'){
 throw 'Exit heading handoff must run after native platform placement and passenger removal.'
}
& (Join-Path $fixture 'test-head-attachment.ps1') -KitRoot $KitRoot -OutDir ($OutDir+'-head')
& (Join-Path $fixture 'test-vehicle-seat-recenter.ps1') -StageRoot $KitRoot -OutDir ($OutDir+'-seat')
