# Compatibility entry point for Xbox preparation; uses the common wizard.
[CmdletBinding()]
param([string]$GameDir, [string]$WorkDir = 'C:\LCVRBuild\modern-assets',
      [string]$OutputDir, [string]$XboxArchive, [string]$XboxSource,
      [string]$AndroidSdk, [string]$Serial, [switch]$BuildOnly,
      [switch]$AcceptDownloads, [switch]$NonInteractive)
& (Join-Path $PSScriptRoot 'prepare-modern-models.ps1') @PSBoundParameters -Profile Xbox
if (-not $?) { exit 1 }
