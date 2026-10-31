param(
    [Parameter(Mandatory=$true)][string]$StageRoot,
    [string]$OutDir
)
$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw (Join-Path $StageRoot 'src/vehicles/Boat.cpp')
$start = $source.IndexOf('CBoat::Render()')
$start = $source.IndexOf('if (!CVehicle::bWheelsOnlyCheat)', $start)
$end = $source.IndexOf('#ifdef NEW_RENDERER', $start)
if ($start -lt 0 -or $end -le $start) { throw 'Missing boat body render block.' }
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'boat-window-production.inc'),
    $source.Substring($start, $end-$start), [Text.UTF8Encoding]::new($false))
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -OutDir $OutDir -Sources @('test-boat-window-depth.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Boat window depth regression failed.' }
