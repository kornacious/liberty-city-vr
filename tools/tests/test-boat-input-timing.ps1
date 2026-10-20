param(
    [Parameter(Mandatory=$true)][string]$StageRoot,
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lcvr-boat-input-' + [Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference = 'Stop'
$boat = [IO.File]::ReadAllText((Join-Path $StageRoot 'src/vehicles/Boat.cpp'))
$match = [regex]::Match($boat, '(?m)^void\s+CBoat::ProcessControlInputs\([^)]*\)\s*\{')
if (-not $match.Success) { throw 'Missing production boat input function.' }
$end = $match.Index + $match.Length; $depth = 1
while ($depth -gt 0 -and $end -lt $boat.Length) {
    if ($boat[$end] -eq '{') { $depth++ }
    elseif ($boat[$end] -eq '}') { $depth-- }
    $end++
}
if ($depth -ne 0) { throw 'Unterminated boat input function.' }
$helper = [IO.File]::ReadAllText((Join-Path $StageRoot 'src/core/QuestGameTime.h')).Replace('#include "Timer.h"', '// Timer adapter supplied by fixture.')
$generated = $helper + "`n" + $boat.Substring($match.Index, $end - $match.Index)
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'boat-input-timing-production.inc'), $generated, [Text.UTF8Encoding]::new($false))
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-boat-input-timing.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Boat input timing regression failed.' }
