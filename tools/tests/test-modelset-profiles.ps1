param(
 [string]$KitRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
 [string]$OutDir=(Join-Path ([IO.Path]::GetTempPath()) ('lcvr-modelsets-'+[Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference='Stop'
$core=Join-Path $KitRoot 'overlay/src/core'
if(-not(Test-Path $core)) { $core=Join-Path $KitRoot 'src/core' }
$header=[IO.File]::ReadAllText((Join-Path $core 'ModelSets.h'))
$source=[IO.File]::ReadAllText((Join-Path $core 'ModelSets.cpp'))
$source=[regex]::Replace($source,'(?m)^#include .*\r?\n','')
$source=$source.Replace('_WIN32','MODELSETS_TEST_WINDOWS')
$routing=[IO.File]::ReadAllText((Join-Path $core 'QuestModelsetStreaming.h'))
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'modelsets-production.inc'),$header+"`n"+$source+"`n"+$routing,[Text.UTF8Encoding]::new($false))
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $KitRoot -OutDir $OutDir -Sources @('test-modelset-profiles.cpp')
if($LASTEXITCODE -ne 0){throw 'Modelset routing regression failed.'}
