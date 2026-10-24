param(
    [Parameter(Mandatory=$true)][string]$StageRoot,
    [string]$OutDir=(Join-Path ([IO.Path]::GetTempPath()) ('lcvr-mission-counter-'+[guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference='Stop'
$source=[IO.File]::ReadAllText((Join-Path $StageRoot 'src/renderer/Hud.cpp'))
$start=$source.IndexOf("`t`t`tif (CUserDisplay::OnscnTimer.m_sEntries[0].m_bCounterProcessed) {")
$end=$source.IndexOf("`n`t`t/////////////////////////////////",$start)
if($start -lt 0 -or $end -lt 0){throw 'Mission counter production block missing'}
# Include the real counter path, excluding the enclosing OnscnTimer closing brace.
$block=$source.Substring($start,$end-$start).TrimEnd()
$block=$block.Substring(0,$block.LastIndexOf('}'))
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'mission-counter-production.inc'),$block,[Text.UTF8Encoding]::new($false))
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-mission-counter-layout.cpp')
if($LASTEXITCODE -ne 0){throw 'Mission counter layout regression failed'}
