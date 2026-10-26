param([string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lc-crouch-view-'+[Guid]::NewGuid().ToString('N'))))
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $repo -OutDir $OutDir -Sources @('test-vr-crouch-view.cpp')
if($LASTEXITCODE -ne 0){throw 'Crouch camera tests failed.'}
