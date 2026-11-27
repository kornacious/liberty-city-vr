#Requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9.-]+$')][string]$ReleaseTag,
    [Parameter(Mandatory)][string]$SettingsFile,
    [Parameter(Mandatory)][string]$SevenZip,
    [Parameter(Mandatory)][string]$SevenZipLicense,
    [Parameter(Mandatory)][string]$SevenZipSource,
    [Parameter(Mandatory)][string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
if ((!$IsWindows -and !$IsLinux) -or
    [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture -ne 'X64') {
    throw 'Run the release starter on native Windows x64 or Linux x64.'
}

function Invoke-Checked {
    param([string]$Executable, [string[]]$Arguments)
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Executable exited with code $LASTEXITCODE." }
}

$repository = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$rid = if ($IsWindows) { 'win-x64' } else { 'linux-x64' }
$target = if ($IsWindows) { 'Windows-PCVR-x64.zip' } else { 'Linux-PCVR-x64-experimental.tar.gz' }
$changes = Invoke-Checked git @('-C', $repository, 'status', '--short', '--untracked-files=normal')
if ($changes) { throw 'Start from a clean release checkout.' }
$revision = Invoke-Checked git @('-C', $repository, 'rev-parse', 'HEAD')
$settings = Get-Content -LiteralPath $SettingsFile -Raw | ConvertFrom-Json -AsHashtable
$buildRoot = [IO.Path]::GetFullPath([string]$settings.Workplace)
$output = [IO.Path]::GetFullPath($OutputDirectory)
$source = (Resolve-Path -LiteralPath $SevenZipSource).Path
if ([IO.Path]::GetFileName($source) -notmatch '^7z[0-9]+-src\.tar\.xz$') {
    throw 'Provide the matching upstream 7-Zip source archive.'
}
$releaseWork = Join-Path $buildRoot "pcvr-setup-$ReleaseTag-$rid"
$archive = Join-Path $output "Liberty-City-VR-$ReleaseTag-$target"
if ((Test-Path -LiteralPath $releaseWork) -or (Test-Path -LiteralPath $archive)) {
    throw 'Use fresh release staging and output paths.'
}
$kitParent = Join-Path $releaseWork 'kit'
$kit = Join-Path $kitParent 'Liberty City VR Setup'
$settings.Repository = $repository
$settings.Workplace = Join-Path $releaseWork 'build'
New-Item -ItemType Directory -Path $kit, $output -Force | Out-Null
$config = Join-Path $releaseWork 'build-settings.json'
$settings | ConvertTo-Json -Depth 15 | Set-Content -LiteralPath $config
$tracked = Join-Path $releaseWork 'source.tar'
Invoke-Checked git @('-C', $repository, 'archive', '--format=tar', "--output=$tracked", $revision)
Invoke-Checked tar @('-xf', $tracked, '-C', $kit)

$bundle = Join-Path $kit "PCVR/$rid"
$managerProject = Join-Path $repository 'tools/manager/LibertyManager.csproj'
$setupProject = Join-Path $repository 'tools/setup/LibertySetup.csproj'
New-Item -ItemType Directory -Path (Split-Path $bundle -Parent) -Force | Out-Null
Invoke-Checked dotnet @('run', '--project', $managerProject, '-c', 'Release', '--', '--build-pcvr-setup', $config, $bundle)
$converterName = if ($IsWindows) { 'xbox-convert.exe' } else { 'xbox-convert' }
$converter = Join-Path $settings.Workplace "original-xbox/converter/$converterName"
$setup = Join-Path $kit 'Setup'
$manager = Join-Path $releaseWork 'manager'
foreach ($publish in @(@{ Project=$setupProject; Output=$setup }, @{ Project=$managerProject; Output=$manager })) {
    Invoke-Checked dotnet @('publish', $publish.Project, '-c', 'Release', '-r', $rid, '--self-contained', 'true',
        '-p:RestoreLockedMode=true', '-o', $publish.Output)
}
foreach ($file in Get-ChildItem -LiteralPath $manager -Filter 'LibertyManager*' -File) {
    Copy-Item -LiteralPath $file.FullName -Destination $setup -Force
}
$toolInputs = @{ Kit = $kit; Rid = $rid; XboxConverter = $converter; SevenZip = $SevenZip; SevenZipLicense = $SevenZipLicense }
& (Join-Path $PSScriptRoot 'Stage-SetupTools.ps1') @toolInputs
Copy-Item -LiteralPath $source -Destination (Join-Path $setup "tools/$rid")
$lzokay = Join-Path $repository 'tools/modelsets/vendor/lzokay/LICENSE'
Copy-Item -LiteralPath $lzokay -Destination (Join-Path $setup "tools/$rid/LZOKAY_LICENSE.txt")
if (!$IsWindows) {
    Invoke-Checked chmod @('+x', (Join-Path $kit 'SETUP.sh'), (Join-Path $kit 'MANAGE.sh'),
        (Join-Path $setup 'LibertySetup'), (Join-Path $setup 'LibertyManager'))
}
$engineName = if ($IsWindows) { 'libertyvr-game.exe' } else { 'libertyvr-game' }
Invoke-Checked (Join-Path $bundle "app/$engineName") @('--help')
if ((Invoke-Checked git @('-C', $repository, 'rev-parse', 'HEAD')) -ne $revision -or
    (Invoke-Checked git @('-C', $repository, 'status', '--short', '--untracked-files=normal'))) {
    throw 'The source checkout changed during the build.'
}
$revision | Set-Content -LiteralPath (Join-Path $kit 'SOURCE-REVISION.txt')
"Release $ReleaseTag, $rid, source $revision" | Set-Content -LiteralPath (Join-Path $kit 'PACKAGE-BUILD.txt')
if ($IsWindows) {
    Invoke-Checked tar @('-a', '-cf', $archive, '-C', $kitParent, 'Liberty City VR Setup')
} else {
    Invoke-Checked tar @('-czf', $archive, '-C', $kitParent, 'Liberty City VR Setup')
}
$entries = Invoke-Checked tar @('-tf', $archive)
if ("Liberty City VR Setup/PCVR/$rid/app/$engineName" -notin $entries) {
    throw 'The archive is missing the expected PCVR executable.'
}
$hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $([IO.Path]::GetFileName($archive))" | Set-Content -LiteralPath "$archive.sha256" -Encoding ascii
Get-Item -LiteralPath $archive
