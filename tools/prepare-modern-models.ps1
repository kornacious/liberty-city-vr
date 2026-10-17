# One-click preparation of the optional Modern and Xbox model overlays for
# Liberty City VR from a legal GTA III installation and external source packs.
# The packs are downloaded for the individual user and are never bundled in this repository.
[CmdletBinding()]
param(
    [string]$GameDir,
    [string]$WorkDir = "C:\LCVRBuild\modern-assets",
    [string]$OutputDir,
    [ValidateSet("Xbox", "Modern", "Both")][string]$Profile = "Xbox",
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
    [string]$LogPath = (Join-Path $env:TEMP "LibertyCityVR-Prepare-Modern-Models.log")
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"
$wizardVersion = "0.5.6-lcv-1"

$repoRoot = Split-Path $PSScriptRoot -Parent
$builder = Join-Path $PSScriptRoot "modelsets\build-modern-modelset.ps1"
$xboxBuilder = Join-Path $PSScriptRoot "prepare-xbox-models.ps1"
$questInstaller = Join-Path $PSScriptRoot "install-modern-models.ps1"
$script:transcriptStarted = $false

try {
    $logDirectory = Split-Path -Parent $LogPath
    if (-not [string]::IsNullOrWhiteSpace($logDirectory)) {
        New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
    }
    Start-Transcript -Path $LogPath -Force | Out-Null
    $script:transcriptStarted = $true
} catch {
    Write-Host "Warning: diagnostic logging could not start: $($_.Exception.Message)" -ForegroundColor Yellow
}

function Stop-DiagnosticLog {
    if (-not $script:transcriptStarted) { return }
    try { Stop-Transcript | Out-Null } catch { }
    $script:transcriptStarted = $false
}

function Invoke-NativeChecked {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [string]$FailureMessage = "Command failed"
    )
    $previousErrorAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $FilePath @Arguments
        $nativeExitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorAction
    }
    if ($nativeExitCode -ne 0) {
        throw "$FailureMessage (exit code $nativeExitCode)."
    }
}

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
    if ([string]::IsNullOrWhiteSpace($Requested) -or
        -not (Test-Path -LiteralPath $Requested -PathType Container)) {
        throw "The GTA III folder does not exist: $Requested"
    }
    $resolved = (Resolve-Path -LiteralPath $Requested).Path
    foreach ($relative in @("models\gta3.img", "models\gta3.dir")) {
        if (-not (Test-Path -LiteralPath (Join-Path $resolved $relative) -PathType Leaf)) {
            throw "The selected folder is not a complete original GTA III installation. Missing: $relative"
        }
    }
    return $resolved
}

function Prompt-UserForFileOrFolder {
    param([string]$Title, [string]$Description)
    Write-Host ""
    Write-Host "================================================================" -ForegroundColor Yellow
    Write-Host " MANUAL FILE SELECTION REQUIRED" -ForegroundColor Yellow
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
        Write-Host "Paste or enter the full path to the archive file or extracted folder:" -ForegroundColor Yellow
        $userInput = Read-Host "Path"
        if (-not [string]::IsNullOrWhiteSpace($userInput)) {
            $selectedPath = $userInput.Trim('"').Trim("'")
        }
    }

    if ([string]::IsNullOrWhiteSpace($selectedPath) -or -not (Test-Path -LiteralPath $selectedPath)) {
        throw "No valid file or folder was provided: $selectedPath"
    }

    return (Resolve-Path -LiteralPath $selectedPath).Path
}

function Assert-FreeSpace {
    param([string]$Game, [string]$Work)
    $workRoot = [IO.Path]::GetPathRoot([IO.Path]::GetFullPath($Work))
    $gameRoot = [IO.Path]::GetPathRoot([IO.Path]::GetFullPath($Game))
    $workDrive = New-Object IO.DriveInfo($workRoot)
    $gameDrive = New-Object IO.DriveInfo($gameRoot)
    if ($workRoot -eq $gameRoot) {
        if ($workDrive.AvailableFreeSpace -lt 8GB) {
            Write-Host ("Warning: low free space on {0} ({1:N1} GB available)." -f $workRoot, ($workDrive.AvailableFreeSpace / 1GB)) -ForegroundColor Yellow
        }
    } else {
        if ($workDrive.AvailableFreeSpace -lt 6GB) {
            Write-Host ("Warning: low free space on {0} ({1:N1} GB available)." -f $workRoot, ($workDrive.AvailableFreeSpace / 1GB)) -ForegroundColor Yellow
        }
    }
}

function Ensure-Extracted {
    param([string]$Name, [string]$Archive, [string]$Destination)
    if (Test-Path -LiteralPath $Destination -PathType Container) {
        $existing = @(Get-ChildItem -LiteralPath $Destination -Recurse -File -ErrorAction SilentlyContinue)
        if ($existing.Count -gt 10) {
            Write-Host "Reusing existing extracted $Name source: $Destination ($($existing.Count) files)" -ForegroundColor Green
            return $Destination
        }
    }
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    Write-Host "Extracting $Name ($Archive)..." -ForegroundColor Cyan
    $tar = Get-Command tar.exe -ErrorAction SilentlyContinue
    if ($tar) {
        & $tar.Source -xf $Archive -C $Destination 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) { return $Destination }
    }
    if ($Archive.EndsWith(".zip", [StringComparison]::OrdinalIgnoreCase)) {
        Expand-Archive -LiteralPath $Archive -DestinationPath $Destination -Force
        return $Destination
    }
    # Check for 7z
    $sevenZipPaths = @("7z.exe", "C:\Program Files\7-Zip\7z.exe", "C:\Program Files (x86)\7-Zip\7z.exe")
    foreach ($cand in $sevenZipPaths) {
        $cmd = Get-Command $cand -ErrorAction SilentlyContinue
        if ($cmd -or (Test-Path -LiteralPath $cand)) {
            $exe = if ($cmd) { $cmd.Source } else { $cand }
            & $exe x -y "-o$Destination" $Archive | Out-Null
            if ($LASTEXITCODE -eq 0) { return $Destination }
        }
    }
    throw "Could not extract '$Archive'. Extract it manually to '$Destination'."
}

function Ensure-Download-Or-Prompt {
    param(
        [string]$Name,
        [string[]]$Urls,
        [string]$Destination,
        [string]$Provided
    )
    # 1. Check if user provided an archive or folder
    if (-not [string]::IsNullOrWhiteSpace($Provided)) {
        $providedPath = [IO.Path]::GetFullPath($Provided.Trim('"'))
        if (Test-Path -LiteralPath $providedPath) {
            Write-Host "Using user-provided $($Name): $providedPath" -ForegroundColor Green
            return $providedPath
        }
    }

    # 2. Check if already downloaded locally
    if (Test-Path -LiteralPath $Destination -PathType Leaf) {
        $len = (Get-Item -LiteralPath $Destination).Length
        if ($len -gt 100KB) {
            Write-Host "Found existing download for $($Name): $Destination ($([Math]::Round($len/1MB,1)) MB)" -ForegroundColor Green
            Write-Host "Reusing local download (skipping redownload)." -ForegroundColor Green
            return $Destination
        }
    }
    # Also check if any matching archive is already in downloads
    $dlDir = Split-Path -Parent $Destination
    $candidates = @(Get-ChildItem -Path $dlDir -File -ErrorAction SilentlyContinue | Where-Object {
        $_.Extension -match '^\.(zip|rar|7z)$' -and $_.Length -gt 100KB -and
        ($_.Name -like "*$($Name.Split(' ')[0])*" -or $_.Name -like "*xbox*" -or $_.Name -like "*modern*")
    })
    if ($candidates.Count -gt 0) {
        $found = $candidates[0].FullName
        Write-Host "Found existing archive in downloads: $found" -ForegroundColor Green
        Write-Host "Reusing local file (skipping redownload)." -ForegroundColor Green
        return $found
    }

    # 3. Attempt download if URLs are provided
    $curl = Get-Command curl.exe -ErrorAction SilentlyContinue
    if ($curl -and $Urls -and $Urls.Count -gt 0) {
        foreach ($url in $Urls) {
            try {
                Write-Host "Downloading $Name from remote source..." -ForegroundColor Cyan
                $partial = "$Destination.partial"
                if (Test-Path -LiteralPath $partial) { Remove-Item -LiteralPath $partial -Force }
                & $curl.Source --fail --location --retry 3 --output $partial $url
                if ($LASTEXITCODE -eq 0 -and (Test-Path -LiteralPath $partial) -and (Get-Item -LiteralPath $partial).Length -gt 100KB) {
                    Move-Item -LiteralPath $partial -Destination $Destination -Force
                    Write-Host "Download completed successfully: $Destination" -ForegroundColor Green
                    return $Destination
                }
            } catch {
                Write-Host "Download attempt failed: $($_.Exception.Message)" -ForegroundColor Yellow
            }
        }
    }

    # 4. If automatic download failed, ask user to provide the files manually!
    if ($NonInteractive) {
        throw "Automatic download for '$Name' failed or was unavailable, and -NonInteractive is active. Supply local archive path."
    }
    $manual = Prompt-UserForFileOrFolder -Title "Select $Name Archive or Folder" `
        -Description "Automatic download for '$Name' could not be completed from the remote source.`nPlease select or enter the path to the downloaded archive (.zip, .rar, .7z) or folder."
    return $manual
}

try {
    Write-Host "Liberty City VR - Download, Build and Install Models ($wizardVersion)" -ForegroundColor Green
    Write-Host "Required input: only a legal original GTA III PC installation."
    Write-Host "Handles Xbox Assets (Vehicles, Characters and Pedestrians, Weapons, and World/Environment) and Modern overlays."
    Write-Host ""

    $game = Resolve-GameFolder -Requested $GameDir
    $work = [IO.Path]::GetFullPath($WorkDir)
    New-Item -ItemType Directory -Path $work -Force | Out-Null
    Assert-FreeSpace -Game $game -Work $work

    if ($XboxOnly) { $Profile = "Xbox" }
    if ($ModernOnly) { $Profile = "Modern" }
    if ($SkipXbox) { $Profile = "Modern" }

    # Route 1: XBOX profile (Vehicles, Characters, Weapons, World/Environment)
    if ($Profile -eq "Xbox" -or $Profile -eq "Both") {
        Write-Host "==> PREPARING XBOX ASSETS PROFILE..." -ForegroundColor Cyan
        $xboxOptions = @{
            GameDir = $game
            WorkDir = $work
            BuildOnly = $BuildOnly
            AcceptDownloads = $AcceptDownloads
            NonInteractive = $NonInteractive
        }
        if ($XboxArchive) { $xboxOptions.XboxArchive = $XboxArchive }
        if ($XboxSource) { $xboxOptions.XboxSource = $XboxSource }
        if ($AndroidSdk) { $xboxOptions.AndroidSdk = $AndroidSdk }
        if ($Serial) { $xboxOptions.Serial = $Serial }
        if ($OutputDir -and $Profile -eq "Xbox") { $xboxOptions.OutputDir = $OutputDir }

        & $xboxBuilder @xboxOptions
        if (-not $?) { throw "Xbox profile preparation failed." }

        if ($Profile -eq "Xbox") {
            Write-Host ""
            Write-Host "XBOX ASSETS INSTALLATION COMPLETED." -ForegroundColor Green
            Write-Host "Fully restart Liberty City VR on the headset."
            Write-Host "In VR MENU > SETTINGS > MODEL ASSETS, Xbox profile is active for Vehicles, Characters, Weapons, and World."
            Write-Host "Diagnostic log: $LogPath"
            Stop-DiagnosticLog
            exit 0
        }
    }

    # Route 2: Modern profile
    if ($Profile -eq "Modern" -or $Profile -eq "Both") {
        Write-Host "==> PREPARING MODERN ASSETS PROFILE..." -ForegroundColor Cyan
        $modernOutput = if ($OutputDir -and $Profile -eq "Modern") { [IO.Path]::GetFullPath($OutputDir) } else { Join-Path $game "modelsets\modern" }
        
        $downloads = Join-Path $work "downloads"
        $sources = Join-Path $work "sources"
        New-Item -ItemType Directory -Path $downloads, $sources -Force | Out-Null

        $targetModern = Join-Path $downloads "GTA3_Modern_HD.zip"
        $modernSource = Ensure-Download-Or-Prompt -Name "GTA III Modern / HD Pack" `
            -Urls @("https://drive.usercontent.google.com/download?id=1Swe1dVWDnKz8ad51y8L0ihPWVCxmFRYj&export=download&confirm=t") `
            -Destination $targetModern -Provided (if ($ModernArchive) { $ModernArchive } else { $HdArchive })

        $extractedModern = if (Test-Path -LiteralPath $modernSource -PathType Container) {
            $modernSource
        } else {
            Ensure-Extracted -Name "Modern HD Pack" -Archive $modernSource -Destination (Join-Path $sources "modern-pack")
        }

        # Build overlay into $modernOutput
        & (Join-Path $PSScriptRoot "modelsets/build-xbox-modelset.ps1") -Source $extractedModern -Out $modernOutput -Force
        if (-not $?) { throw "Modern overlay build failed." }

        if (-not $BuildOnly) {
            $installOpts = @{ ModernDir = $modernOutput; Profile = "Modern"; NonInteractive = $NonInteractive }
            if ($AndroidSdk) { $installOpts.AndroidSdk = $AndroidSdk }
            if ($Serial) { $installOpts.Serial = $Serial }
            & $questInstaller @installOpts
            if (-not $?) { throw "Quest Modern model installation failed." }
        }
    }

    Write-Host ""
    Write-Host "DOWNLOAD, BUILD AND QUEST INSTALL COMPLETED." -ForegroundColor Green
    Write-Host "Fully restart Liberty City VR on the headset."
    Write-Host "Diagnostic log: $LogPath"
    Stop-DiagnosticLog
    exit 0
} catch {
    Write-Host ""
    Write-Host "ERROR: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Diagnostic log: $LogPath"
    Stop-DiagnosticLog
    exit 1
}
