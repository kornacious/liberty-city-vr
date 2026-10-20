param(
    [Parameter(Mandatory=$true)][ValidateSet('Record','Replay','Stop','Status','Pull')][string]$Action,
    [Parameter(Mandatory=$true)][string]$Serial,
    [ValidatePattern('^[A-Za-z0-9_-]{1,32}$')][string]$Label='trial',
    [ValidateRange(1,120)][int]$Seconds=30,
    [string]$OutDir,
    [string]$Adb='adb'
)
$ErrorActionPreference='Stop'
$remote='/sdcard/Android/data/com.libertyvr.quest/files/vehicle-capture'
function Invoke-Adb([string[]]$Arguments){
    $result=& $Adb -s $Serial @Arguments
    if($LASTEXITCODE-ne0){throw "ADB command failed: $($Arguments[0])"}
    $result
}
$model=(Invoke-Adb -Arguments @('shell','getprop','ro.product.model') | Out-String).Trim()
if($model-ne'Quest 3'){throw "Expected a verified Quest 3; $Serial reports '$model'."}
if($Action-eq'Status'){Invoke-Adb -Arguments @('shell','cat',"$remote/status.txt");exit}
if($Action-eq'Pull'){$command='stop'}
elseif($Action-eq'Record'){$command="record $Label $Seconds"}
elseif($Action-eq'Replay'){$command="replay $Label"}
else{$command='stop'}
Invoke-Adb -Arguments @('shell','mkdir','-p',$remote) | Out-Null
$taskRoot=Split-Path $PSScriptRoot -Parent
$scratch=Join-Path $taskRoot '.cache/vehicle-capture-commands'
[void][IO.Directory]::CreateDirectory($scratch)
$local=Join-Path $scratch ([Guid]::NewGuid().ToString('N')+'.txt')
[IO.File]::WriteAllText($local,$command+"`n",[Text.UTF8Encoding]::new($false))
Invoke-Adb -Arguments @('push',$local,"$remote/control.pending") | Out-Null
Invoke-Adb -Arguments @('shell','mv',"$remote/control.pending","$remote/control.txt") | Out-Null
$deadline=[DateTime]::UtcNow.AddSeconds(6)
do{
    & $Adb -s $Serial shell test -e "$remote/control.txt"
    if($LASTEXITCODE-ne0){break}
    Start-Sleep -Milliseconds 250
}while([DateTime]::UtcNow-lt$deadline)
if([DateTime]::UtcNow-ge$deadline){throw 'Command remains pending: run a capture-enabled APK in unpaused gameplay. No input replay has been confirmed.'}
$status=(Invoke-Adb -Arguments @('shell','cat',"$remote/status.txt") | Out-String).Trim()
Write-Host $status
if($Action-eq'Pull'){
    if($status-ne'stopped'){throw 'Capture did not close cleanly; inspect status before pulling.'}
    if(-not$OutDir){$OutDir=Join-Path $taskRoot ('.cache/vehicle-traces/'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss'))}
    if(Test-Path -LiteralPath $OutDir){throw 'Choose a new output directory.'}
    [void][IO.Directory]::CreateDirectory($OutDir)
    Invoke-Adb -Arguments @('pull',"$remote/.",[IO.Path]::GetFullPath($OutDir))
}elseif(($Action-eq'Record'-and$status-notin@('recording','duration complete'))-or($Action-eq'Replay'-and$status-notlike'replaying:*'-and$status-ne'replay complete')){
    throw "Requested $Action did not start: $status"
}
