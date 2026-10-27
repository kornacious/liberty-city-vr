param(
    [Parameter(Mandatory=$true)][string]$Re3Root,
    [string]$KitRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$OutDir = (Join-Path $KitRoot ('.cache/lcvr-regressions-' + [guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference = 'Stop'
$KitRoot = [IO.Path]::GetFullPath($KitRoot)
$OutDir = [IO.Path]::GetFullPath($OutDir)
if (Test-Path -LiteralPath $OutDir) { throw 'Choose a new output directory.' }
# Reject a reVC input before assembling: these adapters intentionally target
# Liberty City's actual inventory and APIs, never invented compatibility enums.
$weaponTypes = Get-Content -Raw (Join-Path $Re3Root 'src/weapons/WeaponType.h')
if ($weaponTypes -notmatch 'WEAPONTYPE_HELICANNON' -or $weaponTypes -match 'WEAPONTYPE_KATANA') {
    throw 'Re3Root must be the GTA III/re3 upstream documented in BUILDING.md.'
}
[void][IO.Directory]::CreateDirectory($OutDir)
$stage = Join-Path $OutDir 'source'
& (Join-Path $KitRoot 'tools/assemble.ps1') -Re3 $Re3Root -Out $stage
if (-not (Test-Path -LiteralPath (Join-Path $stage 'src/peds/PlayerPed.cpp'))) { throw 'Assembly failed.' }
$kitTests = @('android-audio','cd-stream-sync','laser-cache','mip-worker','hd-texture-loading','modelset-profiles','perf-capture','physical-scope','quest-crouch','radio-touch','ragdoll','vr-crouch-view','vulkan-descriptors','vulkan-pipelines','xr-focus-input')
$engineTests = @(
    'cd-directory','traffic-visibility','plane-visibility','mission-selector','mission-counter-layout','radio-station',
    'autosave','frame-timers','angular-drag-boat-lift','wheel-suspension-fps','vehicle-coupled-fps','vehicle-capture','boat-input-timing','physics-sleep-timing','heli-control-timing','gameplay-timing','cheat-menu','graphics-menu-layout','never-wanted','player-debug-gates',
    'quest-melee','quest-sprint','held-weapon-frame','holster-inventory','throwable-preview','wheel-hand-basis','weapon-grip','weapon-cadence','ragdoll-brake-hook','ragdoll-explosion',
    'ragdoll-lifecycle','ragdoll-menu','ragdoll-off-cost','ragdoll-platform',
    'ragdoll-presentation','ragdoll-ray-resolver','ragdoll-schedule','ragdoll-weight',
    'vehicle-deformation-hitframe','vehicle-deformation-menu','vehicle-deformation-mesh',
    'vehicle-deformation-runtime','vehicle-deformation-slots','vehicle-seat-recenter',
    'vr-cop-aim','vr-vehicle-entry','world-billboards'
)
# Explicit exclusions are evidence, never successful tests. See README.md for
# reference-only fixtures and diagnostics requiring local assets/toolchains.
$reference = @(
    'devtools-culling','heli-bullets','modelset-wheels',
    'ragdoll-vehicle-hook','ragdoll-visibility','vehicle-deformation-collision','vehicle-driveby'
)
$results = @()
$pwsh = (Get-Process -Id $PID).Path
foreach ($name in @($kitTests + $engineTests)) {
    $script = Join-Path $PSScriptRoot ('test-' + $name + '.ps1')
    $parameters = (Get-Command $script).Parameters
    $arguments = @('-NoProfile','-NonInteractive','-File',$script)
    if ($engineTests -contains $name) { $arguments += @('-StageRoot',$stage) }
    if ($parameters.ContainsKey('KitRoot')) { $arguments += @('-KitRoot',$KitRoot) }
    if ($parameters.ContainsKey('OutDir')) { $arguments += @('-OutDir',(Join-Path $OutDir $name)) }
    $log = Join-Path $OutDir ($name + '.log')
    & $pwsh @arguments *> $log
    $code = $LASTEXITCODE
    $status = if ($code -eq 0) { 'passed' } else { 'failed' }
    $results += [pscustomobject]@{ test=$name; status=$status; exitCode=$code; log=$log }
    Write-Host ($name + ': ' + $status)
}
$failed = @($results | Where-Object status -eq 'failed')
$report = [ordered]@{
    kit=$KitRoot; assembledSource=$stage
    sourceHashes=@('src/core/Wanted.h','src/peds/PlayerPed.cpp','src/vr/QuestWeaponVR.cpp' | ForEach-Object {
        Get-FileHash -LiteralPath (Join-Path $stage $_) -Algorithm SHA256 | Select-Object Path,Hash
    })
    results=$results
    referenceOnly=$reference
    notRun='Asset diagnostics, shader-tool diagnostics, installer fixtures and optional historical comparison modes; see tools/tests/README.md.'
    passed=@($results | Where-Object status -eq 'passed').Count
    failed=$failed.Count
}
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutDir 'results.json')
Write-Host ("Current-source suites: $($report.passed) passed, $($report.failed) failed; $($reference.Count) reference-only fixtures excluded.")
Write-Host ("Logs and report: $OutDir")
if ($failed.Count) { throw ('Failed suites: ' + (($failed | ForEach-Object test) -join ', ')) }
