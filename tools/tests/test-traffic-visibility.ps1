param(
    [Parameter(Mandatory=$true)][string]$StageRoot,
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lcvr-traffic-' + [Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw (Join-Path $StageRoot 'src/control/CarCtrl.cpp')
# Verify the tested policy is wired into generation and both cleanup paths.
foreach ($pattern in @(
    'preferredDistance = QuestTrafficVisibility::SpawnDistance',
    'if \(!QuestTrafficVisibility::CanSpawn\(',
    'threshold = Max\(threshold, QuestTrafficVisibility::RetentionDistance',
    'Magnitude2D\(\) >\s*QuestTrafficVisibility::RetentionDistance'
)) {
    if ($source -notmatch $pattern) { throw "Traffic policy integration missing: $pattern" }
}
$kit = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $kit -OutDir $OutDir -Sources @('test-traffic-visibility.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Traffic visibility regression failed.' }
