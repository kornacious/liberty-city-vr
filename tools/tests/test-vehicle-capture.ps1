param(
    [Parameter(Mandatory=$true)][string]$StageRoot,
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lcvr-capture-' + [Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference='Stop'
$runtime=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/vehicles/VehicleCapture.cpp'))
$runtime=[regex]::Replace($runtime,'(?m)^#include "[^\r\n]+"\r?\n','')
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'vehicle-capture-runtime.inc'),$runtime,[Text.UTF8Encoding]::new($false))
foreach($pair in @(
    @('src/core/World.cpp','VehicleCaptureFrame vehicleCaptureFrame'),
    @('src/vehicles/Automobile.cpp','VehicleCaptureInputs\(this\)'),
    @('src/vehicles/Automobile.cpp','VehicleCaptureCollision\(this, numCollisions\)'),
    @('src/entities/Physical.cpp','VehicleCaptureSubsteps\(this, n\)'))){
    if([IO.File]::ReadAllText((Join-Path $StageRoot $pair[0]))-notmatch$pair[1]){throw "Missing capture integration: $($pair[0])"}
}
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-vehicle-capture.cpp','test-vehicle-capture-off.cpp')
if($LASTEXITCODE-ne0){throw 'Vehicle capture/replay regression failed.'}
