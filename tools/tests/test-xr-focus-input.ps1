param(
    [string]$KitRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lcvr-focus-input-' + [Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference = 'Stop'
$xrRoot = Join-Path $KitRoot 'overlay/android/app/src/main/cpp'
$session = [IO.File]::ReadAllText((Join-Path $xrRoot 'xr_vulkan_session.cpp'))
$header = [IO.File]::ReadAllText((Join-Path $xrRoot 'xr_vulkan_session.h'))
$app = [IO.File]::ReadAllText((Join-Path $xrRoot 'android_main.cpp'))
$android = [IO.File]::ReadAllText((Join-Path $KitRoot 'overlay/src/skel/android/android.cpp'))
$androidHeader = [IO.File]::ReadAllText((Join-Path $KitRoot 'overlay/src/skel/android/android.h'))
$weapons = [IO.File]::ReadAllText((Join-Path $KitRoot 'overlay/src/vr/QuestWeaponVR.cpp'))
function Extract([string]$Source, [string]$Pattern) {
    $match = [regex]::Match($Source, $Pattern)
    if (-not $match.Success) { throw "Cannot extract production focus-input code: $Pattern" }
    return $match.Value
}
$types = Extract $header '(?ms)^struct TrackedPose\r?\n\{.*?^\};'
$types += "`n" + (Extract $header '(?ms)^struct ControllerInput\r?\n\{.*?^\};')
$types += "`nnamespace androidgame {`n" + (Extract $androidHeader '(?ms)^struct PadInput\r?\n\{.*?^\};') + "`n}`n"
$code = ''
foreach ($name in @('readFloat','readBool','readStick','clearInput','syncInput','locateTrackedPose','isPoseActionActive','locateControllerPoses','getInput')) {
    $code += (Extract $session ('(?ms)^(?:void|float|bool)\r?\n' + $name + '\(.*?^\}')) + "`n"
}
$transition = Extract $session '(?ms)^\t{3}g.sessionState = changed.state;.*?(?=^\t{3}if\(g.sessionState == XR_SESSION_STATE_READY)'
$code += "void SessionEvent(XrSessionState state) { XrEventDataSessionStateChanged changed{}; changed.state = state;`n" + $transition + "}`n"
$entry = Extract $session '(?ms)(?<=renderFrame\(void\)\r?\n\{\r?\n).*?(?=\tXrFrameWaitInfo waitInfo)'
$code += "void StartFrame() {`n" + $entry + "}`n"
$logic = Extract $app '(?ms)^\t{3}if\(resumed && gameStarted\)\{.*?^\t{3}\}'
$code += "void LogicOnly(bool resumed,bool gameStarted) {`n" + $logic + "}`n"
$code += "namespace androidgame {`n" + (Extract $android '(?ms)^void\r?\nSetPadInput\(.*?^\}') + "`n}`n"
$code += (Extract $weapons '(?ms)^static void\r?\nUpdateWeaponTriggerEdges\(.*?^\}') + "`n"
$release = Extract $weapons '(?ms)^\t{2}if\(gPoseValid\[hand\] && gHeldSlot\[hand\] >= 0 && gGrip\[hand\] <= 0.30f\)\{.*?(?=^\t{2}if\(gHeldSlot\[hand\] < 0)'
$code += "void ReleaseWeapon(int hand) { do {`n" + $release + "} while(false); }`n"
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'xr-focus-input-types.inc'), $types)
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'xr-focus-input-production.inc'), $code)
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $KitRoot -OutDir $OutDir -Sources @('test-xr-focus-input.cpp')
