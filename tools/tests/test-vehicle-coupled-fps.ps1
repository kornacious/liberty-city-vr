param(
    [Parameter(Mandatory=$true)][string]$StageRoot,
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lcvr-coupled-' + [Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference='Stop'
$StageRoot=[IO.Path]::GetFullPath($StageRoot);$OutDir=[IO.Path]::GetFullPath($OutDir)
function Extract([string]$s,[string]$pattern){
    $m=[regex]::Match($s,$pattern);if(-not$m.Success){throw "Missing production definition: $pattern"}
    $end=$m.Index+$m.Length;$depth=1
    while($depth-gt0-and$end-lt$s.Length){if($s[$end]-eq'{'){$depth++}elseif($s[$end]-eq'}'){$depth--};$end++}
    if($depth-ne0){throw 'Unterminated production definition.'}
    $s.Substring($m.Index,$end-$m.Index)
}
$physical=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/entities/Physical.cpp'))
$header=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/entities/Physical.h'))
$vehicle=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/vehicles/Vehicle.cpp'))
$collision=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/collision/Collision.cpp'))
$matrix=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/math/Matrix.cpp'))
$auto=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/vehicles/Automobile.cpp'))
$config=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/core/config.h'))
$output=[regex]::Match($header,'(?m)^#define GRAVITY[^\r\n]+').Value+"`n"
foreach($macro in @('GTA3_PC_11','GTA_VERSION')){
    $definition=[regex]::Match($config,('(?m)^#define '+$macro+'\s+[^\r\n]+'))
    if(-not$definition.Success){throw "Missing production configuration: $macro"}
    $output+=$definition.Value+"`n"
}
if([regex]::IsMatch($config,'(?m)^\s*#\s*define FIX_SIGNIFICANT_BUGS\b')){$output+="#define FIX_SIGNIFICANT_BUGS`n"}
foreach($name in @('GetSpeed','ApplyMoveSpeed','ApplyTurnSpeed','ApplyMoveForce','ApplyTurnForce','ApplySpringCollision','ApplySpringDampening','ApplyGravity','ApplyAirResistance','ApplyCollisionAlt')){
    $output+=(Extract $physical ('(?m)^(?:CVector|void|bool)\s+CPhysical::'+$name+'\([^)]*\)\s*\{'))+"`n"
}
$output+=(Extract $header 'float GetMass\([^)]*\)\s*\{').Replace('float GetMass','float CPhysical::GetMass')+"`n"
$output+=(Extract $matrix '(?m)^void\s+CMatrix::Reorthogonalise\(void\)\s*\{')+"`n"
foreach($name in @('ProcessLineBox','ProcessSphereBox')){
    $output+=(Extract $collision ('(?m)^bool\s+CCollision::'+$name+'\([^)]*\)\s*\{'))+"`n"
}
$output+=[regex]::Match($vehicle,'(?m)^float fBurstSpeedMax[^;]+;\s*float fBurstTyreMod[^;]+;').Value+"`n"
$output+=(Extract $vehicle '(?m)^void\s+CVehicle::ProcessWheel\([^)]*\)\s*\{')+"`n"
$timer=[regex]::Match($auto,'(?s)if\(m_aSuspensionSpringRatio\[i\] < 1.0f\)\s*m_aWheelTimer\[i\] = 4.0f;\s*else\s*m_aWheelTimer\[i\] = Max\([^;]+;')
if(-not$timer.Success){throw 'Missing contact timer.'}
$output+='static void ContactTimer(float ratio,float &timer){int i=0;float m_aSuspensionSpringRatio[1]={ratio},m_aWheelTimer[1]={timer};'+$timer.Value+'timer=m_aWheelTimer[0];}'+"`n"
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'vehicle-coupled-fps-production.inc'),$output,[Text.UTF8Encoding]::new($false))
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-vehicle-coupled-fps.cpp')
if($LASTEXITCODE-ne0){throw 'Coupled vehicle fixture failed.'}
& (Join-Path $OutDir 'test-vehicle-coupled-fps.exe') --matrix (Join-Path $OutDir 'measurements.csv')
if($LASTEXITCODE-ne0){throw 'Coupled vehicle characterization failed.'}
$rows=Import-Csv (Join-Path $OutDir 'measurements.csv')
foreach($group in ($rows | Where-Object fixed_hz -ne '0' | Group-Object scenario,fixed_hz)){
    $states=@($group.Group | ForEach-Object {$_ | Select-Object -Property * -ExcludeProperty fps,pacing,wall_s | ConvertTo-Json -Compress} | Select-Object -Unique)
    if($states.Count-ne1){throw "Fixed-step outcomes depend on render pacing: $($group.Name)"}
}
foreach($scenario in @('hard-landing','wall-impact')){
    if(@($rows | Where-Object {$_.scenario-eq$scenario-and[int]$_.body_contacts-gt0}).Count-eq0){throw "Scenario did not exercise body collision: $scenario"}
}
foreach($scenario in ($rows.scenario | Select-Object -Unique)){
    $reference=$rows | Where-Object {$_.scenario-eq$scenario-and$_.fps-eq'30'-and$_.pacing-eq'steady'-and$_.fixed_hz-eq'0'}
    $fixed=$rows | Where-Object {$_.scenario-eq$scenario-and$_.fps-eq'30'-and$_.pacing-eq'steady'-and$_.fixed_hz-eq'30'}
    $a=$reference | Select-Object -Property * -ExcludeProperty fixed_hz | ConvertTo-Json -Compress
    $b=$fixed | Select-Object -Property * -ExcludeProperty fixed_hz | ConvertTo-Json -Compress
    if($a-ne$b){throw "Fixed 30 Hz fixture differs from native 30 FPS reference: $scenario"}
}
Write-Host 'PASS: fixed-step metrics match across render rates/pacing, and hard landing/wall cases exercise native body contacts.'
Write-Host 'PASS: fixed 30 Hz reproduces the native 30 FPS fixture reference in every scenario.'
@('src/entities/Physical.cpp','src/entities/Physical.h','src/vehicles/Vehicle.cpp','src/vehicles/Automobile.cpp','src/collision/Collision.cpp','src/math/Matrix.cpp','src/core/config.h') | ForEach-Object{
    Get-FileHash -LiteralPath (Join-Path $StageRoot $_) | Select-Object Path,Hash
} | ConvertTo-Json | Set-Content (Join-Path $OutDir 'source-hashes.json')
