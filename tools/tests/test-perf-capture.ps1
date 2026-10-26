param(
    [string]$KitRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lcvr-perf-' + [Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $KitRoot -OutDir $OutDir -Sources @('test-perf-capture.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Performance capture host regression failed.' }
