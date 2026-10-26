# Prepare and optionally install Xbox/Modern from the pinned ModDB re3 release.
[CmdletBinding()]
param(
    [string]$GameDir,
    [string]$WorkDir = 'C:\LCVRBuild\modern-assets',
    [string]$OutputDir,
    [ValidateSet('Xbox', 'Modern', 'Both')][string]$Profile = 'Modern',
    [string]$AndroidSdk,
    [string]$Serial,
    [string]$XboxArchive,
    [string]$XboxSource,
    [string]$HdArchive,
    [string]$ModsArchive,
    [string]$ModernArchive,
    [switch]$SkipXbox,
    [switch]$XboxOnly,
    [switch]$ModernOnly,
    [switch]$BuildOnly,
    [switch]$AcceptDownloads,
    [switch]$NonInteractive,
    [string]$LogPath = (Join-Path $env:TEMP 'LibertyCityVR-Prepare-Modern-Models.log')
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'device-diagnostics.ps1')
. (Join-Path $PSScriptRoot 'modelsets/xbox-modelset.ps1')
try {
    Initialize-PrivateDiagnosticLog
    if ($ModsArchive) { throw 'The old two-pack Modern workflow is retired. Supply the re3 pack using -XboxArchive or -ModernArchive.' }
    if ($XboxOnly) { $Profile = 'Modern' }
    if ($ModernOnly -or $SkipXbox) { $Profile = 'Modern' }
    if ($Profile -ne 'Modern') { Write-Host 'Xbox/Both are legacy aliases; preparing the single Modern (Xbox HD) profile.' }
    $Profile = 'Modern'
    if (-not $GameDir) {
        if ($NonInteractive) { throw '-GameDir is required in non-interactive mode.' }
        $GameDir = Read-Host 'Original GTA III PC folder'
    }
    $game = (Resolve-Path -LiteralPath $GameDir).Path
    foreach ($rel in @('models/gta3.img', 'models/gta3.dir')) {
        if (-not (Test-Path -LiteralPath (Join-Path $game $rel) -PathType Leaf)) { throw "Original GTA III installation is missing $rel" }
    }
    $archive = $XboxArchive
    if (-not $archive) { $archive = $ModernArchive }
    if (-not $archive) { $archive = $HdArchive }
    $profiles = @('Modern')
    Write-Host "Preparing $Profile assets. Existing headset profiles stay in place until a verified replacement is ready."
    Write-Host "Work folder: $WorkDir"
    Write-Host "Diagnostic log: $LogPath"
    # Finish and validate all requested builds before contacting a device.
    $outputs = @{}
    foreach ($target in $profiles) {
        $name = $target.ToLowerInvariant()
        $out = if ($OutputDir) { [IO.Path]::GetFullPath($OutputDir) } else { Join-Path $game "modelsets/$name" }
        Write-Host "==> Preparing $target profile: $out"
        $arguments = @('prepare', '--profile', $name, '--out', $out, '--work-dir', $WorkDir)
        if ($XboxSource) { $arguments += @('--source', $XboxSource) }
        if ($archive) { $arguments += @('--archive', $archive) }
        if ($AcceptDownloads) { $arguments += '--accept-downloads' }
        if ($NonInteractive) { $arguments += '--non-interactive' }
        Invoke-ModelsetPython -Arguments $arguments
        $outputs[$target] = $out
    }
    if (-not $BuildOnly) {
        foreach ($target in $profiles) {
            $options = @{ ModernDir = $outputs[$target]; Profile = $target; NonInteractive = $NonInteractive }
            if ($AndroidSdk) { $options.AndroidSdk = $AndroidSdk }
            if ($Serial) { $options.Serial = $Serial }
            & (Join-Path $PSScriptRoot 'install-modern-models.ps1') @options
            if (-not $?) { throw "$target installation failed." }
        }
        Write-Host 'Requested profiles installed. Select Classic or Modern in MODEL ASSETS, then fully restart. Saved selections were preserved.'
    } else {
        Write-Host 'Requested profiles built and verified; no device commands were sent.'
    }
} catch {
    Write-Host ("ERROR: " + (Protect-DeviceDiagnostic $_.Exception.Message)) -ForegroundColor Red
    exit 1
} finally {
    $script:diagnosticLogEnabled = $false
}
