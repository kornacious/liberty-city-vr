param([Parameter(Mandatory=$true)][string]$StageRoot,
 [string]$KitRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
 [string]$OutDir=(Join-Path ([IO.Path]::GetTempPath()) ('lcvr-particle-alpha-'+[Guid]::NewGuid().ToString('N'))))
$ErrorActionPreference='Stop'
$source=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/renderer/Particle.cpp'))
$render=$source.Substring($source.IndexOf('void CParticle::Render()'))
$render=$render.Substring(0,$render.IndexOf('void CParticle::RemovePSystem'))
$begin=[regex]::Match($render,'(?ms)^\s*const bool xboxParticles =.*?^#endif').Value
$blend=[regex]::Match($render,'(?ms)^\s*if \( \(flags & DRAW_OPAQUE\).*?^\t\t\t}').Value
$end=[regex]::Match($render,'(?ms)^#ifdef RW_VULKAN\r?\n\s*rw::SetRenderState\(rw::ALPHATESTFUNC, savedAlphaTest\);\r?\n#endif').Value
if(-not $begin -or -not $blend -or -not $end){throw 'Missing production particle alpha blocks.'}
$fixture=Join-Path $KitRoot ('.cache/particle-alpha-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
Copy-Item (Join-Path $PSScriptRoot 'test-particle-alpha.cpp') $fixture
[IO.File]::WriteAllText((Join-Path $fixture 'particle-alpha-begin.inc'),$begin)
[IO.File]::WriteAllText((Join-Path $fixture 'particle-alpha-blend.inc'),$blend)
[IO.File]::WriteAllText((Join-Path $fixture 'particle-alpha-end.inc'),$end)
[xml]$resources=Get-Content (Join-Path $KitRoot 'tools/manager/EmbeddedTools.resx') -Raw
$helper=$resources.root.data | Where-Object name -eq 'tools/tests/test-ragdoll.ps1'
$runner=([string]$helper.value).Replace('/nologo /EHsc','/nologo /DRW_VULKAN /EHsc')
[IO.File]::WriteAllText((Join-Path $fixture 'test-ragdoll.ps1'),$runner)
& (Join-Path $fixture 'test-ragdoll.ps1') -KitRoot $KitRoot -OutDir $OutDir -Sources @('test-particle-alpha.cpp')
if($LASTEXITCODE -ne 0){throw 'Particle alpha regression failed.'}
