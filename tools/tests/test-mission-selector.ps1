param(
    [Parameter(Mandatory=$true)][string]$StageRoot,
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lc-missions-' + [guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw (Join-Path $StageRoot 'src/control/ScriptDebug.cpp')
$pieces = foreach ($name in @('TrySwitchToMission', 'SwitchToMission')) {
    $match = [regex]::Match($source, ('(?ms)^(?:bool|void)\r?\nCTheScripts::' + $name + '\(.*?^\}'))
    if (-not $match.Success) { throw "Cannot extract production function $name" }
    $match.Value
}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'mission-selector-production.inc'), ($pieces -join "`n"))
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-mission-selector.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Mission selector regression failed.' }
