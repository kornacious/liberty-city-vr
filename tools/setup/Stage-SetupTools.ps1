param(
    [Parameter(Mandatory)][string]$Kit,
    [Parameter(Mandatory)][ValidateSet('win-x64', 'linux-x64')][string]$Rid,
    [Parameter(Mandatory)][string]$XboxConverter,
    [Parameter(Mandatory)][string]$SevenZip,
    [Parameter(Mandatory)][string]$SevenZipLicense,
    [string]$DependencyPrefix,
    [string]$ReleaseTag,
    [string]$ReleaseAssets
)

$ErrorActionPreference = 'Stop'
$windows = $Rid -eq 'win-x64'
if ($windows -ne $IsWindows -and $PSVersionTable.PSEdition -eq 'Core') {
    throw 'Stage and test the Setup tools on their native host.'
}
if ($PSVersionTable.PSEdition -ne 'Core' -and -not $windows) {
    throw 'Use PowerShell 7 on Linux to stage the Linux kit.'
}

$destination = Join-Path $Kit "Setup/tools/$Rid"
New-Item -ItemType Directory -Path $destination -Force | Out-Null
function Copy-Tool($source, $name) {
    $target = Join-Path $destination $name
    Copy-Item -LiteralPath $source -Destination $target
    if (-not $windows) {
        & chmod +x -- $target
        if ($LASTEXITCODE -ne 0) { throw "Could not make $name executable." }
    }
    (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash |
        Set-Content -LiteralPath "$target.sha256" -Encoding ascii
}

$converterName = if ($windows) { 'xbox-convert.exe' } else { 'xbox-convert' }
$archiveToolName = if ($windows) { '7za.exe' } else { '7zz' }
foreach ($check in @('--self-test', '--head-uv-test')) {
    & $XboxConverter $check
    if ($LASTEXITCODE -ne 0) { throw "Xbox converter $check failed." }
}
Copy-Tool $XboxConverter $converterName
Copy-Tool $SevenZip $archiveToolName
& (Join-Path $destination $archiveToolName) i | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Use a standalone 7-Zip console binary that runs without companion DLLs.' }
Copy-Item -LiteralPath $SevenZipLicense -Destination (Join-Path $destination '7-Zip-LICENSE.txt')

if ($DependencyPrefix -or $ReleaseTag -or $ReleaseAssets) {
    if (-not $DependencyPrefix -or -not $ReleaseAssets -or $ReleaseTag -notmatch '^[a-zA-Z0-9._-]+$') {
        throw 'Provide DependencyPrefix, ReleaseAssets and a simple ReleaseTag together.'
    }
    $prefix = (Resolve-Path -LiteralPath $DependencyPrefix).Path
    $extension = if ($windows) { 'zip' } else { 'tar.gz' }
    $archiveName = "libertyvr-desktop-deps-$ReleaseTag-$Rid.$extension"
    New-Item -ItemType Directory -Path $ReleaseAssets -Force | Out-Null
    $archive = Join-Path (Resolve-Path -LiteralPath $ReleaseAssets).Path $archiveName
    if ($windows) {
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        [System.IO.Compression.ZipFile]::CreateFromDirectory($prefix, $archive)
    } else {
        & tar -czf $archive -C $prefix .
        if ($LASTEXITCODE -ne 0) { throw 'Could not create the desktop library archive.' }
    }
    @{
        Url = "https://github.com/kornacious/liberty-city-vr/releases/download/$ReleaseTag/$archiveName"
        Sha256 = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $destination 'desktop-deps.json') -Encoding ascii
}
