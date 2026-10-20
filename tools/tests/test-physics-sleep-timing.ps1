param(
    [Parameter(Mandatory=$true)][string]$StageRoot,
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lcvr-sleep-' + [Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference='Stop'
function Extract([string]$s,[string]$pattern){
    $m=[regex]::Match($s,$pattern)
    if(-not $m.Success){throw "Missing production definition: $pattern"}
    $end=$m.Index+$m.Length;$depth=1
    while($depth -gt 0 -and $end -lt $s.Length){if($s[$end]-eq'{'){$depth++}elseif($s[$end]-eq'}'){$depth--};$end++}
    if($depth-ne0){throw 'Unterminated production block.'}
    $s.Substring($m.Index,$end-$m.Index)
}
$physical=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/entities/Physical.cpp'))
$auto=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/vehicles/Automobile.cpp'))
$clock=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/core/QuestGameTime.h')).Replace('#include "Timer.h"','// Fixture timer adapter.')
$helperPath=Join-Path $StageRoot 'src/core/QuestPhysicsSleep.h'
if(-not(Test-Path $helperPath)){$helperPath=Join-Path $PSScriptRoot '../../overlay/src/core/QuestPhysicsSleep.h'}
$helper=[IO.File]::ReadAllText($helperPath).Replace('#include "QuestGameTime.h"','// Extracted gameplay clock above.').Replace('#include "World.h"','// Fixture world adapter.')
$functions=(Extract $physical '(?m)^void\s+CPhysical::ProcessControl\(void\)\s*\{')+"`n"+
    'bool CAutomobile::CheckSleep(){ bool skipPhysics=false;'+"`n"+
    (Extract $auto 'if\(!bIsStuck && \(GetStatus\(\) == STATUS_ABANDONED \|\| GetStatus\(\) == STATUS_WRECKED\)\)\s*\{')+"`nreturn skipPhysics; }`n"
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'physics-sleep-timing-production.inc'),$clock+"`n"+$helper+"`n"+$functions,[Text.UTF8Encoding]::new($false))
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-physics-sleep-timing.cpp')
if($LASTEXITCODE-ne0){throw 'Physics sleep timing regression failed.'}
# Every displacement producer/reset must update the reference-distance cache.
foreach($s in @($physical,$auto)){
    foreach($m in [regex]::Matches($s,'m_fDistanceTravelled = [^;]+;')){
        $tail=$s.Substring($m.Index+$m.Length)
        if($tail -notmatch '^\s*#if defined\(__ANDROID__\) && defined\(FIX_BUGS\)\s*m_fSleepDistance = QuestPhysicsSleep::ReferenceDistance\(m_fDistanceTravelled\);'){
            throw 'Displacement producer/reset is missing sleep-distance integration.'
        }
    }
}
$header=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/entities/Physical.h'))
if($header-notmatch 'union \{ float m_phys_unused1; float m_fSleepDistance; \}') {throw 'Sleep cache must reuse existing storage.'}
Write-Host 'PASS: all displacement producers/resets update the sleep cache without changing object layout.'
