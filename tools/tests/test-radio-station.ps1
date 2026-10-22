param(
    [Parameter(Mandatory=$true)][string]$StageRoot,
    [string]$KitRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$OutDir=(Join-Path ([IO.Path]::GetTempPath()) ('lcvr-radio-station-'+[guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference='Stop'
$radio=[IO.File]::ReadAllText((Join-Path $KitRoot 'overlay/src/vr/QuestRadio.cpp'))
$music=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/audio/MusicManager.cpp'))
function Extract([string]$text,[string]$pattern) {
    $match=[regex]::Match($text,$pattern)
    if(-not $match.Success){throw "Missing production code: $pattern"}
    return $match.Value
}
$station=Extract $radio '(?ms)^void Station\(.*?^}'
$set=Extract $music '(?ms)^void\r?\ncMusicManager::SetRadioInCar\(.*?^}'
$tuning=Extract $music '(?ms)^uint8\r?\ncMusicManager::GetNextCarTuning\(.*?^}'
$change=Extract $music '(?ms)^bool8\r?\ncMusicManager::ChangeRadioChannel\(.*?^}'
# Keep the native delayed-retune handling and exact-station dispatch intact.
$start=$music.IndexOf("`t`t// Because when you switch radio back and forth")
$end=$music.IndexOf("`t`tif (m_nNextTrack < RADIO_OFF)",$start)
if($start -lt 0 -or $end -lt 0){throw 'Missing ServiceGameMode retune dispatch'}
$dispatch=$music.Substring($start,$end-$start)
$code=$set+"`n"+$tuning+"`n"+$change+"`nnamespace QuestRadio {`n"+$station+"`n}`nvoid cMusicManager::ServiceRetune() {`n"+$dispatch+"`n}"
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'radio-station-production.inc'),$code,[Text.UTF8Encoding]::new($false))
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-radio-station.cpp')
if($LASTEXITCODE -ne 0){throw 'Radio station regression failed'}
