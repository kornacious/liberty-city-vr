param([string]$KitRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
 [string]$OutDir=(Join-Path ([IO.Path]::GetTempPath()) ('lcvr-physical-turn-'+[Guid]::NewGuid().ToString('N'))))
$ErrorActionPreference='Stop'
$source=[IO.File]::ReadAllText((Join-Path $KitRoot 'overlay/src/skel/android/android.cpp'))
$fixture=Join-Path $KitRoot ('.cache/physical-turn-input-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
Copy-Item (Join-Path $PSScriptRoot 'test-physical-turn-input.cpp') $fixture
foreach($part in @(@('gate','const bool onFootGameplay =','if(onFootGameplay){'),@('stick','float moveStickX =','// The stock frontend'))){
 $start=$source.IndexOf($part[1]);$end=$source.IndexOf($part[2],$start)
 if($start -lt 0 -or $end -lt 0){throw 'Missing production input block.'}
 [IO.File]::WriteAllText((Join-Path $fixture ('physical-turn-'+$part[0]+'.inc')),$source.Substring($start,$end-$start))
}
[xml]$resources=Get-Content (Join-Path $KitRoot 'tools/manager/EmbeddedTools.resx') -Raw
$helper=$resources.root.data | Where-Object name -eq 'tools/tests/test-ragdoll.ps1'
[IO.File]::WriteAllText((Join-Path $fixture 'test-ragdoll.ps1'),[string]$helper.value)
& (Join-Path $fixture 'test-ragdoll.ps1') -KitRoot $KitRoot -OutDir $OutDir -Sources @('test-physical-turn-input.cpp')
if($LASTEXITCODE -ne 0){throw 'Physical turn input regression failed.'}
