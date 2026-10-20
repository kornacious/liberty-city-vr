param(
    [Parameter(Mandatory=$true)][string]$StageRoot,
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lcvr-wheel-fps-' + [Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference = 'Stop'
$StageRoot = [IO.Path]::GetFullPath($StageRoot)
$OutDir = [IO.Path]::GetFullPath($OutDir)
function Extract-Block([string]$text, [string]$pattern) {
    $match = [regex]::Match($text, $pattern)
    if (-not $match.Success) { throw "Missing production definition: $pattern" }
    $end = $match.Index + $match.Length; $depth = 1
    while ($depth -gt 0 -and $end -lt $text.Length) {
        if ($text[$end] -eq '{') { $depth++ }
        elseif ($text[$end] -eq '}') { $depth-- }
        $end++
    }
    if ($depth -ne 0) { throw 'Unterminated production definition.' }
    $text.Substring($match.Index, $end - $match.Index)
}
$physical = [IO.File]::ReadAllText((Join-Path $StageRoot 'src/entities/Physical.cpp'))
$header = [IO.File]::ReadAllText((Join-Path $StageRoot 'src/entities/Physical.h'))
$vehicle = [IO.File]::ReadAllText((Join-Path $StageRoot 'src/vehicles/Vehicle.cpp'))
$auto = [IO.File]::ReadAllText((Join-Path $StageRoot 'src/vehicles/Automobile.cpp'))
$config = [IO.File]::ReadAllText((Join-Path $StageRoot 'src/core/config.h'))
$output = [regex]::Match($header, '(?m)^#define GRAVITY[^\r\n]+').Value + "`n"
if ([regex]::IsMatch($config, '(?m)^\s*#\s*define FIX_SIGNIFICANT_BUGS\b')) { $output += "#define FIX_SIGNIFICANT_BUGS`n" }
foreach ($name in @('GetSpeed','ApplyMoveSpeed','ApplyMoveForce','ApplyTurnForce','ApplySpringCollision','ApplySpringDampening','ApplyGravity','ApplyAirResistance')) {
    $output += (Extract-Block $physical ('(?m)^(?:CVector|void|bool)\s+CPhysical::' + $name + '\([^)]*\)\s*\{')) + "`n"
}
$mass = Extract-Block $header 'float GetMass\([^)]*\)\s*\{'
$output += $mass.Replace('float GetMass', 'float CPhysical::GetMass') + "`n"
$constants = [regex]::Match($vehicle, '(?m)^float fBurstSpeedMax[^;]+;\s*float fBurstTyreMod[^;]+;')
if (-not $constants.Success) { throw 'Missing burst tyre constants.' }
$output += $constants.Value + "`n" + (Extract-Block $vehicle '(?m)^void\s+CVehicle::ProcessWheel\([^)]*\)\s*\{') + "`n"
$timer = [regex]::Match($auto, '(?s)if\(m_aSuspensionSpringRatio\[i\] < 1.0f\)\s*m_aWheelTimer\[i\] = 4.0f;\s*else\s*m_aWheelTimer\[i\] = Max\([^;]+;')
if (-not $timer.Success) { throw 'Missing wheel contact grace period.' }
$output += "static void ProductionContactTimer(float ratio, float &timer) { int i=0; float m_aSuspensionSpringRatio[1]={ratio}; float m_aWheelTimer[1]={timer};`n" + $timer.Value + "`ntimer=m_aWheelTimer[0]; }`n"
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'wheel-suspension-fps-production.inc'), $output, [Text.UTF8Encoding]::new($false))
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-wheel-suspension-fps.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Wheel fixture compilation/smoke checks failed.' }
$exe = Join-Path $OutDir 'test-wheel-suspension-fps.exe'
$rows = [Collections.Generic.List[string]]::new()
$rows.Add('scenario,fps,pacing,wall_s,sim_s,stop_s,distance_m,forward_mps,lateral_mps,heading_deg,min_height_m,max_height_m,peak_vertical_mps,skid_fraction,locked_fraction,sign_reversals,contact_grace_ms,final_height_m,residual_vertical_span_m')
foreach ($scenario in @('brake','coast','lateral','steer','drop','bump','grace','history-clean','history-primed')) {
    foreach ($fps in @(15,18,20,30,36,60,72,80,90,120,144)) {
        foreach ($pacing in @('steady','jitter','stall')) {
            # Fresh process: native ProcessWheel statics must not leak between
            # scenarios/frame rates. The history pair intentionally probes them.
            $line = & $exe $scenario $fps $pacing
            if ($LASTEXITCODE -ne 0) { throw "Failed: $scenario $fps $pacing" }
            $rows.Add(($line -join ''))
        }
    }
}
[IO.File]::WriteAllLines((Join-Path $OutDir 'measurements.csv'), $rows)
$hashes = @('src/entities/Physical.cpp','src/entities/Physical.h','src/vehicles/Vehicle.cpp','src/vehicles/Automobile.cpp','src/core/config.h') | ForEach-Object {
    Get-FileHash -LiteralPath (Join-Path $StageRoot $_) | Select-Object Path,Hash
}
$hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutDir 'source-hashes.json')
Write-Host ('PASS: ' + ($rows.Count-1) + ' isolated controlled-rate scenarios; measurements: ' + (Join-Path $OutDir 'measurements.csv'))
