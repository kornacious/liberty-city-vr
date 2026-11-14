param(
    [string]$KitRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$OutDir = (Join-Path $KitRoot ('.cache/neo-gloss-' + [Guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference = 'Stop'
$fixture = $OutDir + '-fixture'
[void][IO.Directory]::CreateDirectory($fixture)
$source = Get-Content (Join-Path $KitRoot 'librw/src/vulkan/vkpipe.cpp') -Raw
$begin = $source.IndexOf("`tpush.ambientLight[0] = push.ambientLight[1]")
$end = $source.IndexOf("`t// Which of the frame's dynamic lights", $begin)
if ($begin -lt 0 -or $end -le $begin) { throw 'Cannot extract production lighting setup.' }
[IO.File]::WriteAllText((Join-Path $fixture 'neo-gloss-production.inc'), $source.Substring($begin, $end-$begin))
Copy-Item (Join-Path $PSScriptRoot 'test-neo-gloss.cpp') $fixture
[xml]$resources = Get-Content (Join-Path $KitRoot 'tools/manager/EmbeddedTools.resx') -Raw
$helper = $resources.root.data | Where-Object name -eq 'tools/tests/test-ragdoll.ps1'
if (-not $helper) { throw 'Manager native test compiler setup is missing.' }
[IO.File]::WriteAllText((Join-Path $fixture 'test-ragdoll.ps1'), [string]$helper.value)
& (Join-Path $fixture 'test-ragdoll.ps1') -KitRoot $KitRoot -OutDir $OutDir -Sources @('test-neo-gloss.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Neo gloss regression failed.' }
