param(
    [Parameter(Mandatory=$true)][string]$StageRoot,
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lcvr-gameplay-timing-' + [Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference='Stop'
function Extract([string]$s,[string]$pattern){
    $m=[regex]::Match($s,$pattern)
    if(-not$m.Success){throw "Missing production code: $pattern"}
    $end=$m.Index+$m.Length;$depth=1
    while($depth-gt0-and$end-lt$s.Length){if($s[$end]-eq'{'){$depth++}elseif($s[$end]-eq'}'){$depth--};$end++}
    if($depth-ne0){throw 'Unterminated definition.'}
    $s.Substring($m.Index,$end-$m.Index)
}
$door=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/vehicles/Door.cpp'))
$plane=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/vehicles/Plane.cpp'))
$timer=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/core/Timer.cpp')).Replace("`r`n","`n")
$start=$timer.IndexOf("#else`n`nuint32 CTimer::m_snTimeInMilliseconds;")
if($start-lt0){throw 'Missing non-PS2 timer.'}
$timer=$timer.Substring($start+6);$timer=$timer.Substring(0,$timer.LastIndexOf('#endif'))
$explosion=[regex]::Match($plane,'(?s)\t// Explosion\s*(.*?)\s*// Update plane position and speed')
if(-not$explosion.Success){throw 'Missing production destruction sequence.'}
if(-not[regex]::IsMatch($plane,'m_nFrameWhenHit = CTimer::GetTimeInMilliseconds\(\);\s*plane->m_nDestructionFrame = 0;')){throw 'Plane hit must initialize game-time origin and event progress.'}
$helper=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/core/QuestGameTime.h')).Replace('#include "Timer.h"','// Actual Timer.h is included by this fixture.')
$generated=@{
    'gameplay-timing-timer-header.inc'=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/core/Timer.h'))
    'gameplay-timing-timer.inc'=$timer
    'gameplay-timing-helper.inc'=$helper
    'gameplay-timing-door-header.inc'=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/vehicles/Door.h'))
    'gameplay-timing-door.inc'=(Extract $door '(?m)^CDoor::CDoor\(void\)\s*\{')+"`n"+(Extract $door '(?m)^void\s+CDoor::Process\([^)]*\)\s*\{')
    'gameplay-timing-plane.inc'='void CPlane::ProcessDestruction(){ int i; CVector pos;'+"`n"+$explosion.Groups[1].Value+"`n}"
}
foreach($name in $generated.Keys){[IO.File]::WriteAllText((Join-Path $PSScriptRoot $name),$generated[$name],[Text.UTF8Encoding]::new($false))}
foreach($path in @('src/core/World.cpp','src/vehicles/Automobile.cpp','src/vehicles/Train.cpp','src/control/Garages.cpp')){
    $s=[IO.File]::ReadAllText((Join-Path $StageRoot $path))
    if($s-notmatch'QuestGameTime::Due\('){throw "Missing gameplay schedule integration: $path"}
}
foreach($path in @('src/control/Pickups.cpp','src/peds/Population.cpp')){
    $s=[IO.File]::ReadAllText((Join-Path $StageRoot $path))
    if($s-notmatch'questTick < QuestGameTime::Count\(\)'){throw "Missing crossed-partition integration: $path"}
}
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-gameplay-timing.cpp')
if($LASTEXITCODE-ne0){throw 'Gameplay timing regression failed.'}
