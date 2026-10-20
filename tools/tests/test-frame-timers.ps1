param(
    [string]$StageRoot,
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lcvr-frame-timers-' + [Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference = 'Stop'
$timer = [IO.File]::ReadAllText((Join-Path $StageRoot 'src/core/Timer.cpp'))
# Compile the real non-PS2 timer, including its fractional clock accumulation,
# clipping, pause, time-scale and initialization paths, against a synthetic clock.
$start = $timer.IndexOf("#else`n`nuint32 CTimer::m_snTimeInMilliseconds;")
if ($start -lt 0) {
    $timer = $timer.Replace("`r`n", "`n")
    $start = $timer.IndexOf("#else`n`nuint32 CTimer::m_snTimeInMilliseconds;")
}
if ($start -lt 0) { throw 'Cannot locate the production non-PS2 timer.' }
$body = $timer.Substring($start + 6)
$end = $body.LastIndexOf('#endif')
$body = $body.Substring(0, $end)
$script = [IO.File]::ReadAllText((Join-Path $StageRoot 'src/control/Script.h'))
$update = [regex]::Match($script, '(?s)void UpdateTimers\(float timeStep\)\s*\{[^}]+\}')
if (-not $update.Success) { throw 'Cannot extract the script timer consumer.' }
$car = [IO.File]::ReadAllText((Join-Path $StageRoot 'src/vehicles/Automobile.cpp'))
$drain = [regex]::Match($car, '(?s)// Decrease car health if engine is damaged badly\s*(.*?)\s*ProcessDelayedExplosion\(\);')
if (-not $drain.Success) { throw 'Cannot extract damaged-engine health drain.' }
$generated = @{
    'frame-timer-header.inc' = [IO.File]::ReadAllText((Join-Path $StageRoot 'src/core/Timer.h'))
    'frame-timer-production.inc' = $body
    'frame-timer-script.inc' = $update.Value
    'frame-timer-engine.inc' = $drain.Groups[1].Value
}
foreach ($name in $generated.Keys) {
    [IO.File]::WriteAllText((Join-Path $PSScriptRoot $name), $generated[$name], [Text.UTF8Encoding]::new($false))
}
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-frame-timers.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Frame timer regression failed.' }
