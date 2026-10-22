param(
    [string]$KitRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$OutDir=(Join-Path ([IO.Path]::GetTempPath()) ('lcvr-radio-touch-'+[guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference='Stop'
$radio=[IO.File]::ReadAllText((Join-Path $KitRoot 'overlay/src/vr/QuestRadio.cpp'))
$code='#include "'+((Join-Path ([IO.Path]::GetFullPath($KitRoot)) 'overlay/src/vr/RadioBoardLayout.h').Replace('\','/'))+'"'+"`n"
$code+="namespace QuestRadio {`n"
foreach($name in @('Update','SuppressDriving','MenuRow','MenuAction')) {
    $match=[regex]::Match($radio,'(?m)^(?:void|bool) '+$name+'\(')
    if(-not $match.Success){throw "Missing production function: $name"}
    $open=$radio.IndexOf('{',$match.Index);$depth=0
    for($i=$open;$i -lt $radio.Length;$i++) {
        if($radio[$i] -eq '{'){$depth++}
        if($radio[$i] -eq '}'){$depth--;if($depth -eq 0){break}}
    }
    if($depth -ne 0){throw "Unterminated production function: $name"}
    $code+=$radio.Substring($match.Index,$i-$match.Index+1)+"`n"
}
$code+="}`n"
$hand=[IO.File]::ReadAllText((Join-Path $KitRoot 'overlay/src/vr/VRHandModel.cpp'))
$code+="namespace VRHandModel {`n"
foreach($name in @('Clamp01','InterpolatePose','IndexTip')) {
    $match=[regex]::Match($hand,'(?ms)^(?:float|CVector|bool)\r?\n'+$name+'\(.*?^}')
    if(-not $match.Success){throw "Missing production hand function: $name"}
    $code+=$match.Value+"`n"
}
$code+="}`n"
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'radio-touch-production.inc'),$code,[Text.UTF8Encoding]::new($false))
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $KitRoot -OutDir $OutDir -Sources @('test-radio-touch.cpp')
if($LASTEXITCODE -ne 0){throw 'Radio touch regression failed'}
