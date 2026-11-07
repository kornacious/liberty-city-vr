param(
    [string]$KitRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lcvr-neo-rim-' + [Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference = 'Stop'
# Reuse the Manager's native compiler setup; compile the production header and
# the same scalar ramp included by both Vulkan vertex shaders.
$fixture = Join-Path $KitRoot ('.cache/neo-rim-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
Copy-Item (Join-Path $PSScriptRoot 'test-neo-rim.cpp') $fixture
[xml]$resources = Get-Content (Join-Path $KitRoot 'tools/manager/EmbeddedTools.resx') -Raw
$helper = $resources.root.data | Where-Object name -eq 'tools/tests/test-ragdoll.ps1'
if (-not $helper) { throw 'Manager native test compiler setup is missing.' }
[IO.File]::WriteAllText((Join-Path $fixture 'test-ragdoll.ps1'), [string]$helper.value)
& (Join-Path $fixture 'test-ragdoll.ps1') -KitRoot $KitRoot -OutDir $OutDir -Sources @('test-neo-rim.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Neo rim regression failed.' }
