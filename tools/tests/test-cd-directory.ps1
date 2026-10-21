param(
    [Parameter(Mandatory=$true)][string]$StageRoot,
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) ('lcvr-directory-' + [guid]::NewGuid().ToString('N')))
)
$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw (Join-Path $StageRoot 'src/core/Directory.cpp')
$header = Get-Content -Raw (Join-Path $StageRoot 'src/core/Directory.h')
$production = ''
foreach ($pattern in @(
    '(?ms)^CDirectory::CDirectory\(.*?^}',
    '(?ms)^CDirectory::~CDirectory\(.*?^}',
    '(?ms)^void\r?\nCDirectory::AddItem\(.*?^}',
    '(?ms)^bool\r?\nCDirectory::FindItem\(.*?^}'
)) {
    $matches = [regex]::Matches($source, $pattern)
    if (-not $matches.Count) { throw "Cannot extract production directory code: $pattern" }
    foreach ($match in $matches) { $production += $match.Value + "`n" }
}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'cd-directory-types.inc'), $header)
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'cd-directory-production.inc'), $production)
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -OutDir $OutDir -Sources @('test-cd-directory.cpp','test-cd-directory-no-dedup.cpp')
