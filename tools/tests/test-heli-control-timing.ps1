param(
    [Parameter(Mandatory=$true)][string]$StageRoot,
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lcvr-heli-' + [Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference='Stop'
function Extract([string]$s,[string]$pattern){
    $m=[regex]::Match($s,$pattern);if(-not$m.Success){throw "Missing production code: $pattern"}
    $end=$m.Index+$m.Length;$depth=1
    while($depth-gt0-and$end-lt$s.Length){if($s[$end]-eq'{'){$depth++}elseif($s[$end]-eq'}'){$depth--};$end++}
    if($depth-ne0){throw 'Unterminated production block.'}
    $s.Substring($m.Index,$end-$m.Index)
}
$heli=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/vehicles/Heli.cpp'))
$clock=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/core/QuestGameTime.h')).Replace('#include "Timer.h"','// Fixture timer.')
if($heli-match'CHeli::ProcessAvoidance\('){
    $avoid=Extract $heli '(?m)^void\s+CHeli::ProcessAvoidance\([^)]*\)\s*\{'
}else{
    $old=[regex]::Match($heli,'(?s)(\tif\(m_heliStatus == HELI_STATUS_HOVER2 \|\| m_heliStatus == HELI_STATUS_SHOT_DOWN\)\{.*?)\s*if\(m_heliType == HELI_TYPE_CATALINA\)')
    if(-not$old.Success){throw 'Missing legacy helicopter avoidance.'}
    $avoid='void CHeli::ProcessAvoidance(CVector2D &targetSpeed,const CVector2D &vTargetDist){'+$old.Groups[1].Value+'}'
}
$landing='void CHeli::Landing(CVector target){'+(Extract $heli 'if\(m_pathState == 6\)\s*\{')+'}'
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'heli-control-timing-production.inc'),$clock+"`n"+$avoid+"`n"+$landing,[Text.UTF8Encoding]::new($false))
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-heli-control-timing.cpp')
if($LASTEXITCODE-ne0){throw 'Helicopter control timing regression failed.'}
if($heli-notmatch'ProcessAvoidance\(targetSpeed, vTargetDist\);'-or$heli-notmatch'm_fAvoidanceSideSpeed = 0.0f;'){
    throw 'Missing control integration or constructor initialization.'
}
