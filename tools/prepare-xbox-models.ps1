# Prepares and installs the complete Xbox Assets profile for Liberty City VR
# (Vehicles, characters and pedestrians, weapons, and world/environment).
[CmdletBinding()]
param(
    [Parameter(Mandatory=$false)][string]$GameDir,
    [string]$WorkDir = "C:\LCVRBuild\modern-assets",
    [string]$OutputDir,
    [string]$XboxArchive,
    [string]$XboxSource,
    [string]$AndroidSdk,
    [string]$Serial,
    [switch]$BuildOnly,
    [switch]$AcceptDownloads,
    [switch]$NonInteractive
)
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "modelsets/xbox-modelset.ps1")

function Resolve-GameFolder {
    param([string]$Requested)
    if ([string]::IsNullOrWhiteSpace($Requested)) {
        if ($NonInteractive) { throw "-GameDir is required in non-interactive mode." }
        try {
            Add-Type -AssemblyName System.Windows.Forms
            $dialog = New-Object System.Windows.Forms.FolderBrowserDialog
            $dialog.Description = "Select the original GTA III PC installation folder"
            $dialog.ShowNewFolderButton = $false
            if ($dialog.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) {
                throw "No GTA III folder was selected."
            }
            $Requested = $dialog.SelectedPath
        } catch [System.Management.Automation.RuntimeException] {
            throw
        } catch {
            Write-Host "Folder picker was unavailable. Paste the original GTA III folder path." -ForegroundColor Yellow
            $Requested = Read-Host "GTA III folder"
        }
    }
    if ([string]::IsNullOrWhiteSpace($Requested) -or -not (Test-Path -LiteralPath $Requested -PathType Container)) {
        throw "The GTA III folder does not exist: $Requested"
    }
    $resolved = (Resolve-Path -LiteralPath $Requested).Path
    foreach ($relative in @("models/gta3.img", "models/gta3.dir")) {
        if (-not (Test-Path -LiteralPath (Join-Path $resolved $relative) -PathType Leaf)) {
            throw "The selected folder is not a complete original GTA III installation. Missing: $relative"
        }
    }
    return $resolved
}

function Prompt-UserForManualSource {
    param([string]$Title, [string]$Description)
    Write-Host ""
    Write-Host "================================================================" -ForegroundColor Yellow
    Write-Host " MANUAL XBOX ASSETS SELECTION REQUIRED" -ForegroundColor Yellow
    Write-Host "================================================================" -ForegroundColor Yellow
    Write-Host $Description -ForegroundColor Cyan
    Write-Host ""

    $selectedPath = $null
    try {
        Add-Type -AssemblyName System.Windows.Forms -ErrorAction Stop
        $dialog = New-Object System.Windows.Forms.OpenFileDialog
        $dialog.Title = $Title
        $dialog.Filter = "Archive files (*.zip;*.rar;*.7z)|*.zip;*.rar;*.7z|All files (*.*)|*.*"
        $dialog.CheckFileExists = $true
        Write-Host "Opening file picker window... (check your taskbar if it is not in front)" -ForegroundColor Yellow
        $result = $dialog.ShowDialog()
        if ($result -eq [System.Windows.Forms.DialogResult]::OK -and -not [string]::IsNullOrWhiteSpace($dialog.FileName)) {
            $selectedPath = $dialog.FileName
        }
    } catch { }

    if ([string]::IsNullOrWhiteSpace($selectedPath)) {
        Write-Host "Paste or enter the full path to the Xbox archive file (.zip/.rar/.7z) or extracted folder:" -ForegroundColor Yellow
        $userInput = Read-Host "Xbox Assets path"
        if (-not [string]::IsNullOrWhiteSpace($userInput)) {
            $selectedPath = $userInput.Trim('"').Trim("'")
        }
    }

    if ([string]::IsNullOrWhiteSpace($selectedPath) -or -not (Test-Path -LiteralPath $selectedPath)) {
        throw "No valid file or folder was provided: $selectedPath"
    }

    return (Resolve-Path -LiteralPath $selectedPath).Path
}

function Extract-ArchiveSafe {
    param([string]$Archive, [string]$Destination)
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    Write-Host "Extracting archive: $Archive -> $Destination" -ForegroundColor Cyan
    
    $tar = Get-Command tar.exe -ErrorAction SilentlyContinue
    if ($tar) {
        $tarResult = & $tar.Source -xf $Archive -C $Destination 2>&1
        if ($LASTEXITCODE -eq 0) { return }
    }
    # Check for 7z
    $sevenZipPaths = @("7z.exe", "C:\Program Files\7-Zip\7z.exe", "C:\Program Files (x86)\7-Zip\7z.exe")
    foreach ($cand in $sevenZipPaths) {
        $cmd = Get-Command $cand -ErrorAction SilentlyContinue
        if ($cmd -or (Test-Path -LiteralPath $cand)) {
            $exe = if ($cmd) { $cmd.Source } else { $cand }
            & $exe x -y "-o$Destination" $Archive | Out-Null
            if ($LASTEXITCODE -eq 0) { return }
        }
    }
    # Fallback to Expand-Archive for .zip
    if ($Archive.EndsWith(".zip", [StringComparison]::OrdinalIgnoreCase)) {
        Expand-Archive -LiteralPath $Archive -DestinationPath $Destination -Force
        return
    }
    throw "Could not extract '$Archive'. Please extract it manually and pass the folder with -XboxSource."
}

try {
    Write-Host "Liberty City VR - Xbox Assets Profile Setup" -ForegroundColor Green
    Write-Host "Handles Vehicles, Characters and Pedestrians, Weapons, and World/Environment." -ForegroundColor Green
    Write-Host ""

    $game = Resolve-GameFolder -Requested $GameDir
    if (-not $OutputDir) { $OutputDir = Join-Path $game "modelsets/xbox" }
    $output = [IO.Path]::GetFullPath($OutputDir)

    if (Test-XboxOverlay $output) {
        Write-Host "Reusing the complete verified Xbox profile: $output" -ForegroundColor Green
    } else {
        $work = [IO.Path]::GetFullPath($WorkDir)
        $downloads = Join-Path $work "downloads"
        $sources = Join-Path $work "sources"
        New-Item -ItemType Directory -Path $downloads, $sources -Force | Out-Null

        $sourceDirectory = $null

        # 1. Check if user passed -XboxSource folder
        if ($XboxSource -and (Test-Path -LiteralPath $XboxSource -PathType Container)) {
            $sourceDirectory = (Resolve-Path -LiteralPath $XboxSource).Path
            Write-Host "Using user-specified Xbox source folder: $sourceDirectory" -ForegroundColor Green
        }

        # 2. Check if already extracted in sources\xbox-pack
        $extract = Join-Path $sources "xbox-pack"
        if (-not $sourceDirectory -and (Test-Path -LiteralPath $extract -PathType Container)) {
            $existingExtracted = @(Get-ChildItem -LiteralPath $extract -Recurse -File | Where-Object { $_.Extension -match '^\.(dff|txd)$' })
            if ($existingExtracted.Count -ge 20) {
                Write-Host "Reusing existing extracted Xbox source folder: $extract ($($existingExtracted.Count) models found)" -ForegroundColor Green
                $sourceDirectory = $extract
            }
        }

        # 3. Check for local archives (either specified, or previously downloaded in $downloads)
        $archive = $null
        if (-not $sourceDirectory) {
            if ($XboxArchive -and (Test-Path -LiteralPath $XboxArchive -PathType Leaf)) {
                $archive = (Resolve-Path -LiteralPath $XboxArchive).Path
                Write-Host "Using user-specified local Xbox archive: $archive" -ForegroundColor Green
            } else {
                # Look in downloads for any matching archive
                $localCandidates = @(Get-ChildItem -Path $downloads -File -ErrorAction SilentlyContinue |
                    Where-Object { $_.Extension -match '^\.(zip|rar|7z)$' -and ($_.Name -like "*xbox*" -or $_.Name -like "*fixed*") })
                if ($localCandidates.Count -gt 0) {
                    $archive = $localCandidates[0].FullName
                    Write-Host "Reusing local archive found in downloads (skipping download): $archive" -ForegroundColor Green
                }
            }
        }

        # 4. If neither extracted source nor local archive exists, try automatic download
        if (-not $sourceDirectory -and -not $archive) {
            $candidateUrls = @(
                "https://drive.usercontent.google.com/download?id=1wqUggAT4VJGW6RHJiXhft8Ni8uxe3N5v&export=download&confirm=t"
            )
            $targetDownload = Join-Path $downloads "GTA3_Xbox_Assets.zip"

            if (-not $AcceptDownloads -and -not $NonInteractive) {
                Write-Host "No local Xbox assets archive was detected." -ForegroundColor Yellow
                $ans = Read-Host "Attempt to download the Xbox Assets pack automatically? [Y/n]"
                if ($ans -and $ans -notmatch '^[Yy]') {
                    $archive = $null
                } else {
                    $AcceptDownloads = $true
                }
            }

            if ($AcceptDownloads) {
                $curl = Get-Command curl.exe -ErrorAction SilentlyContinue
                if ($curl) {
                    foreach ($url in $candidateUrls) {
                        try {
                            Write-Host "Attempting download from: $url" -ForegroundColor Cyan
                            $partial = "$targetDownload.partial"
                            if (Test-Path -LiteralPath $partial) { Remove-Item -LiteralPath $partial -Force }
                            & $curl.Source --fail --location --retry 3 --output $partial $url
                            if ($LASTEXITCODE -eq 0 -and (Test-Path -LiteralPath $partial) -and (Get-Item -LiteralPath $partial).Length -gt 100KB) {
                                Move-Item -LiteralPath $partial -Destination $targetDownload -Force
                                $archive = $targetDownload
                                Write-Host "Download completed successfully: $archive" -ForegroundColor Green
                                break
                            }
                        } catch {
                            Write-Host "Automatic download attempt failed." -ForegroundColor Yellow
                        }
                    }
                }
            }

            # 5. If automatic download didn't succeed, ask user to provide the files manually!
            if (-not $archive) {
                if ($NonInteractive) {
                    throw "Automatic download for Xbox Assets was unavailable. Supply -XboxArchive or -XboxSource in non-interactive mode."
                }
                $manual = Prompt-UserForManualSource -Title "Select Xbox Assets Archive or Folder" `
                    -Description "Automatic download could not be completed from the remote source.`nPlease select or enter the path to the Xbox Assets (.zip, .rar, .7z) or extracted folder."
                if (Test-Path -LiteralPath $manual -PathType Container) {
                    $sourceDirectory = $manual
                } else {
                    $archive = $manual
                }
            }
        }

        # 6. Extract archive if sourceDirectory is not yet a folder
        if (-not $sourceDirectory -and $archive) {
            Extract-ArchiveSafe -Archive $archive -Destination $extract
            $sourceDirectory = $extract
        }

        if (-not $sourceDirectory -or -not (Test-Path -LiteralPath $sourceDirectory -PathType Container)) {
            throw "Failed to obtain valid Xbox assets source directory."
        }

        Write-Host "Building Xbox overlay from source: $sourceDirectory" -ForegroundColor Cyan
        & (Join-Path $PSScriptRoot "modelsets/build-xbox-modelset.ps1") -Source $sourceDirectory -Out $output -Force
        if (-not $? -or -not (Test-XboxOverlay $output)) {
            throw "Xbox profile build failed."
        }
    }

    if ($BuildOnly) {
        Write-Host "Xbox profile ready; no device commands were sent." -ForegroundColor Green
        return
    }

    $options = @{ ModernDir = $output; Profile = "Xbox"; NonInteractive = $NonInteractive }
    if ($AndroidSdk) { $options.AndroidSdk = $AndroidSdk }
    if ($Serial) { $options.Serial = $Serial }

    & (Join-Path $PSScriptRoot "install-modern-models.ps1") @options
    if (-not $?) { throw "Xbox profile installation failed." }
} catch {
    Write-Error $_
    exit 1
}
