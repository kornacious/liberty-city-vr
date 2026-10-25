param(
    [string]$KitRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$OutDir = (Join-Path $KitRoot ('out/crouch-tests-' + [guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference = 'Stop'
$body = [IO.File]::ReadAllText((Join-Path $KitRoot 'overlay/src/peds/VrBodyIK.cpp'))
$parts = @()
foreach ($name in @('ToBasisLocal','FromBasisLocal','AlignCrouchedSkeleton')) {
    $match = [regex]::Match($body, ('(?ms)^static (?:CVector|void) ' + $name + '\(.*?^\}'))
    if (-not $match.Success) { throw "Missing production helper $name" }
    $parts += $match.Value
}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'quest-crouch-production.inc'), ($parts -join "`n"))
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $KitRoot -OutDir $OutDir -Sources @('test-quest-crouch.cpp')
