param(
    [Parameter(Mandatory=$true)][string]$StageRoot,
    [string]$OutDir = (Join-Path $env:TEMP ('lcvr-plane-' + [guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference = 'Stop'
$renderer = Get-Content -Raw (Join-Path $StageRoot 'src/renderer/Renderer.cpp')
$helper = [regex]::Match($renderer, '(?s)bool\s+CRenderer::IsEntityOutsideCurrentView\([^)]*\)\s*\{.*?\n\}')
$branch = [regex]::Match($renderer, '(?s)CRenderer::SetupBigBuildingVisibility\(CEntity \*ent\).*?else if \(mi->GetModelType\(\) == MITYPE_VEHICLE\)\s*(.*?)\n\s*float dist')
if (!$helper.Success -or !$branch.Success) { throw 'Cannot extract production plane visibility path.' }
$production = $helper.Value + "`nint CRenderer::PlaneVisibility(CEntity *ent){`n" + $branch.Groups[1].Value + "`n}"
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'plane-visibility-production.inc'), $production)
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-plane-visibility.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Plane visibility regression failed.' }
