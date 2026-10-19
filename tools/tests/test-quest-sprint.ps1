param(
    [string]$StageRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('vc-sprint-' + [Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference = 'Stop'
$path = Join-Path $StageRoot 'src\peds\PlayerPed.cpp'
$source = [IO.File]::ReadAllText($path)
$parts = @(
    '(?ms)^static bool\r?\nIsRunWithoutLimitsCheatEnabled\(.*?(?=^#ifdef VC_PED_PORTS)',
    '(?ms)^void\r?\nCPlayerPed::ProcessAnimGroups\(.*?^\}'
)
$definitions = @()
foreach ($pattern in $parts) {
    $match = [regex]::Match($source, $pattern)
    if (-not $match.Success) { throw "Cannot extract sprint production code: $pattern" }
    $definitions += $match.Value
}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'quest-sprint-production.inc'), ($definitions -join "`n"), [Text.UTF8Encoding]::new($false))
$animHeader = [IO.File]::ReadAllText((Join-Path $StageRoot 'src\animation\AnimManager.h'))
$enum = [regex]::Match($animHeader, '(?ms)^enum AssocGroupId.*?^\};')
if (-not $enum.Success) { throw 'Cannot extract actual animation group enum.' }
$animSource = [IO.File]::ReadAllText((Join-Path $StageRoot 'src\animation\AnimManager.cpp'))
$arrays = [regex]::Matches($animSource, '(?ms)^char const\s*\*\s*aPlayer(?:WithRocket|1Armed|2Armed|BBBat|StrafeBack|StrafeLeft|StrafeRight)?Animations\[\] = \{.*?^\};')
if ($arrays.Count -ne 8) { throw 'Cannot extract eight forward/strafe/back movement animation mappings.' }
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'quest-sprint-anims.inc'), $enum.Value + "`n" + (($arrays | ForEach-Object { $_.Value }) -join "`n"), [Text.UTF8Encoding]::new($false))
Copy-Item -LiteralPath (Join-Path $StageRoot 'src\weapons\WeaponType.h') -Destination (Join-Path $PSScriptRoot 'quest-sprint-weapon-types.inc')
Write-Host ('PlayerPed.cpp SHA256: ' + (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash)
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-quest-sprint.cpp')
