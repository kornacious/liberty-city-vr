param(
    [string]$StageRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lcvr-held-frame-' + [Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference = 'Stop'
$path = Join-Path $StageRoot 'src/peds/VrBodyIK.cpp'
if (-not (Test-Path -LiteralPath $path)) { $path = Join-Path $StageRoot 'overlay/src/peds/VrBodyIK.cpp' }
$source = [IO.File]::ReadAllText($path)
function Extract([string]$pattern) {
    $match = [regex]::Match($source, $pattern)
    if (-not $match.Success) { throw "Cannot extract held-weapon frame production code: $pattern" }
    return $match.Value
}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'held-weapon-context.inc'),
    (Extract '(?ms)^static bool\r?\nBodyIKGameplayContext\(void\).*?(?=^namespace\r?\n\{)'))
$helpers = @(
    (Extract '(?ms)^static CVector ToBasisLocal\(.*?(?=^static bool BuildBikeCarrierBasis)'),
    (Extract '(?ms)^static void CaptureRigidWeaponLink\(.*?(?=^static void OrthonormaliseBasis)'),
    (Extract '(?ms)^static bool GetFloatingHandPresentationPose\(.*?(?=^static bool GetVehicleHandPresentationPose)')
)
$refreshes = [regex]::Matches($source, '(?ms)^\tif\(vehicleIK\)\r?\n\t\t::VrUpdateFirstPersonAnchor\(false\);\r?\n\tOculusVR::RefreshQuestTrackedHandWorldPosesForRender\(\);')
if ($refreshes.Count -ne 2) { throw 'Expected current vehicle anchor refresh in both skeleton paths.' }
if ($source -match 'AdvancePositionBetweenCarriers|gHandCarrier') { throw 'Fresh tracked hand targets must not be carried a second time.' }
for ($i = 0; $i -lt 2; $i++) {
    [IO.File]::WriteAllText((Join-Path $PSScriptRoot "held-weapon-refresh-$i.inc"), $refreshes[$i].Value)
}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'held-weapon-frame-helpers.inc'), ($helpers -join "`n"))
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'held-weapon-frame-apply.inc'),
    (Extract '(?ms)^void Apply\(CPed \*ped\).*?^}\r?\n(?=})'))
Write-Host ('VrBodyIK.cpp SHA256: ' + (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash)
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-held-weapon-frame.cpp')
