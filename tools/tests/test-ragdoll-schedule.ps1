param(
 [string]$StageRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
 [string]$OutDir=(Join-Path ([IO.Path]::GetTempPath()) ('vc-ragdoll-schedule-'+[Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference='Stop'
# The solver fixture calls Update directly. Also check its production caller:
# bodies may relink sectors here, so this must precede native world traversal.
$world=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/core/World.cpp'))
if([regex]::Matches($world,'VrRagdoll::Update\s*\(').Count -ne 1){
 throw 'World processing must call the ragdoll update exactly once.'
}
if(-not[regex]::IsMatch($world,'(?s)CRecordDataForChase::SaveOrRetrieveCarPositions\(\);\s*\} else \{\s*#if defined\(__ANDROID__\) && defined\(GTA_VR_WEAPONS\)\s*(?://[^\r\n]*\r?\n\s*)*VrRagdoll::Update\(\);\s*#endif\s*for\(CPtrNode \*node = ms_listMovingEntityPtrs.first;')){
 throw 'Ragdolls must update in normal Android VR world processing before animation/control/sector traversal, outside cutscene-only processing.'
}
$path=Join-Path $StageRoot 'src\peds\VrRagdoll.cpp'
$text=[IO.File]::ReadAllText($path)
$parts=[Collections.Generic.List[string]]::new()
$structure=[regex]::Match($text,'(?ms)^struct StepContacts \{.*?^\};')
if(-not$structure.Success){throw 'Missing combined contact context.'}
$parts.Add($structure.Value)
foreach($name in @('ProjectContacts','Update')){
 $signature=[regex]::Match($text,'(?m)^(?:static\s+)?void\s+'+$name+'\([^)]*\)\s*\{')
 if(-not$signature.Success){throw "Missing production $name"}
 $end=$signature.Index+$signature.Length;$depth=1
 while($end-lt$text.Length-and$depth-gt0){if($text[$end]-eq'{'){$depth++}elseif($text[$end]-eq'}'){$depth--};$end++}
 if($depth-ne0){throw "Unterminated production $name"}
 $parts.Add($text.Substring($signature.Index,$end-$signature.Index))
}
if(-not[regex]::IsMatch($text,'MAX_POSES\s*=\s*NUMPEDS')){throw 'Retention no longer matches native ped capacity.'}
if(-not[regex]::IsMatch($text,'candidate.pending = slot->waitTime;')){throw 'Fairness must use uncapped waiting time, not capped physics debt.'}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'ragdoll-schedule-production.inc'),($parts-join"`r`n"),[Text.UTF8Encoding]::new($false))
Write-Host ('VrRagdoll.cpp SHA256: '+(Get-FileHash -LiteralPath $path).Hash)
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-ragdoll-schedule.cpp')
