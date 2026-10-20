param(
    [Parameter(Mandatory=$true)][string]$StageRoot,
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lcvr-drag-lift-' + [Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference = 'Stop'
function Extract-Block([string]$source, [string]$pattern) {
    $match = [regex]::Match($source, $pattern)
    if (-not $match.Success) { throw "Cannot extract production block: $pattern" }
    $end = $match.Index + $match.Length
    $depth = 1
    while ($depth -gt 0 -and $end -lt $source.Length) {
        if ($source[$end] -eq '{') { $depth++ }
        elseif ($source[$end] -eq '}') { $depth-- }
        $end++
    }
    if ($depth -ne 0) { throw 'Unterminated production block.' }
    return $source.Substring($match.Index, $end - $match.Index)
}
$physical = [IO.File]::ReadAllText((Join-Path $StageRoot 'src/entities/Physical.cpp'))
$floater = [IO.File]::ReadAllText((Join-Path $StageRoot 'src/vehicles/Floater.cpp'))
$boat = [IO.File]::ReadAllText((Join-Path $StageRoot 'src/vehicles/Boat.cpp'))
$lift = Extract-Block $boat 'if\(!onLand && bBoatInWater && GetUp\(\).z > 0.0f\)\s*\{'
$gravity = [regex]::Match([IO.File]::ReadAllText((Join-Path $StageRoot 'src/entities/Physical.h')), '(?m)^#define GRAVITY[^\r\n]+')
if (-not $gravity.Success) { throw 'Missing production gravity constant.' }
$output = $gravity.Value + "`n" + (Extract-Block $physical '(?m)^void\s+CPhysical::ApplyAirResistance\(void\)\s*\{') + "`n" +
    (Extract-Block $floater '(?m)^bool\s+cBuoyancy::CalcBuoyancyForce\([^)]*\)\s*\{') + "`n" +
    "void CBoat::ExtractedLift(bool onLand, const CVector &buoyancePoint, const CVector &buoyanceImpulse) {`n" + $lift + "`n}`n"
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'angular-drag-boat-lift-production.inc'), $output, [Text.UTF8Encoding]::new($false))
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-angular-drag-boat-lift.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Angular drag / boat lift regression failed.' }
