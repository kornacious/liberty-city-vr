param([string]$KitRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw (Join-Path $KitRoot 'overlay/src/audio/sampman_android.cpp')
function Extract([string]$Pattern) {
    $match = [regex]::Match($source, $Pattern)
    if (-not $match.Success) { throw "Cannot extract production audio code: $Pattern" }
    $match.Value
}
$types = Extract '(?ms)^struct AndroidChannel \{.*?^};'
$code = ''
foreach ($name in @('clampSample', 'updateSpatialization', 'audioCallback')) {
    $code += (Extract ('(?ms)^static (?:int|void|aaudio_data_callback_result_t)\s+' + $name + '\(.*?^}')) + "`n"
}
foreach ($name in @('InitialiseChannel', 'SetChannelEmittingVolume', 'SetChannel3DPosition', 'SetChannel3DDistances', 'SetChannelVolume', 'SetChannelPan', 'SetChannelFrequency', 'SetChannelLoopPoints', 'SetChannelLoopCount', 'StartChannel')) {
    $code += (Extract ('(?ms)^(?:bool8|void)\s+cSampleManager::' + $name + '\(.*?^}')) + "`n"
}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'android-audio-types.inc'), $types)
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'android-audio-production.inc'), $code)
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $KitRoot -Sources @('test-android-audio.cpp')
