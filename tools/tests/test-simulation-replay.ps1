param(
    [Parameter(Mandatory=$true)][string]$StageRoot,
    [Parameter(Mandatory=$true)][string]$OutDir
)
$ErrorActionPreference='Stop'
$runtime=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/vehicles/SimulationReplay.cpp'))
$runtime=[regex]::Replace($runtime,'(?m)^#include "[^\r\n]+"\r?\n','')
$runtime=[regex]::Replace($runtime,'(?m)^#include <dirent.h>\r?\n','')
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'simulation-replay-runtime.inc'),$runtime,[Text.UTF8Encoding]::new($false))
$menu=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/skel/android/vrdebug.cpp'))
$pieces=@()
foreach($name in @('BeginFullVrMenuPage','DrawFullVrMenuRow','DrawQuestSimulationPage')){
    $definition=[regex]::Match($menu,'(?ms)^static [^\r\n]+\r?\n'+$name+'\(.*?^\}')
    if(-not$definition.Success){throw "Missing replay menu function: $name"}
    $pieces+=$definition.Value
}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'simulation-replay-menu.inc'),($pieces -join "`n"),[Text.UTF8Encoding]::new($false))
$hud=[regex]::Match($menu,'(?ms)^const unsigned char \*\r?\nVrSimulationRecordingPixels\(.*?^\}')
if(-not$hud.Success){throw 'Recording HUD implementation missing'}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'simulation-replay-hud.inc'),$hud.Value,[Text.UTF8Encoding]::new($false))
if($menu-notmatch'SimulationReplayConsumeStartFailure\(\)\)\{\s+gVrMenuVisible = true;\s+gVrMenuPage = VR_MENU_PAGE_SIMULATION;'){throw 'Failed starts must reopen the replay menu with status visible'}
foreach($pair in @(
    @('src/core/World.cpp','SimulationReplayFrame simulationReplayFrame'),
    @('src/core/World.cpp','SimulationReplayIgnoreDamage\(pEntity, pCreator\)'),
    @('src/vehicles/Automobile.cpp','SimulationReplayInputs\(this\)'),
    @('src/entities/Physical.cpp','SimulationReplayIgnoreCollision\(A, B\)'),
    @('src/vehicles/Automobile.cpp','SimulationReplayIgnoreCollision\(this, ent\)'),
    @('src/peds/Ped.cpp','SimulationReplayIgnoreCollision\(this, collidingEnt\)'),
    @('src/peds/PedAI.cpp','SimulationReplayProtected\(veh\)'),
    @('src/peds/PedFight.cpp','SimulationReplayIgnoreDamage\(this, damagedBy\)'),
    @('src/vehicles/Vehicle.cpp','SimulationReplayIgnoreDamage\(this, damagedBy\)'),
    @('src/weapons/Weapon.cpp','SimulationReplayProtected\(shooter\)'),
    @('android/app/src/main/cpp/android_main.cpp','QuestProfilerEndAppFrame\(\);\s+androidgame::SimulationReplayAfterAppFrame\(\);'),
    @('src/skel/android/vrdebug.cpp','SimulationReplayMenuAction\(row'))){
    if([IO.File]::ReadAllText((Join-Path $StageRoot $pair[0]))-notmatch$pair[1]){throw "Missing replay integration: $($pair[0])"}
}
$physical=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/entities/Physical.cpp'))
if(([regex]::Matches($physical,'SimulationReplayIgnoreCollision\(A, B\)')).Count-ne3){throw 'Ghost pair filter must cover shift, simple car, and normal collision sector paths.'}
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-simulation-replay.cpp')
if($LASTEXITCODE-ne0){throw 'Simulation replay regression failed.'}
