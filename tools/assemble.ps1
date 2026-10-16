# Assembles a buildable source tree for the Liberty City VR Quest port.
#
#   .\tools\assemble.ps1 -Re3 <path to re3 source> -Out <output dir>
#
# The re3 source tree is not part of this repository and is not distributed
# with it. The tested patch base is commit b050e1cc63b1869e6b514ab2f1a00ef4969e0c84
# from mrxenginner/re3.
param(
    [Parameter(Mandatory=$true)][Alias("Revc")][string]$Re3,
    [Parameter(Mandatory=$true)][string]$Out
)
$ErrorActionPreference = "Stop"
$repo = Split-Path $PSScriptRoot -Parent

if (-not (Test-Path (Join-Path $Re3 "src\core\main.cpp"))) {
    Write-Error "'$Re3' does not look like a re3 source tree (src\core\main.cpp not found)"
}
if (Test-Path $Out) { Write-Error "'$Out' already exists; refusing to overwrite" }

Write-Host "[1/4] Copying the re3 tree..."
New-Item -ItemType Directory -Path $Out | Out-Null
robocopy $Re3 $Out /E /NFL /NDL /NJH /NJS /XD ".git" /XF ".git" | Out-Null
if ($LASTEXITCODE -ge 8) {
    Write-Error "Failed to copy re3 (robocopy exit code $LASTEXITCODE). Remove the partial output and retry."
}

Write-Host "[2/4] Applying the port patches..."
if (Test-Path (Join-Path $Out ".git")) { Remove-Item -Force -Recurse (Join-Path $Out ".git") }
Push-Location $Out
git init -q 2>$null

$patchFiles = Get-ChildItem -Path (Join-Path $repo "patches") -Filter "*.patch" | Sort-Object Name
foreach ($patch in $patchFiles) {
    Write-Host "Applying $($patch.Name)..."
    git apply --whitespace=nowarn $patch.FullName
    if ($LASTEXITCODE -ne 0) {
        Pop-Location
        Write-Error "Patch $($patch.Name) did not apply cleanly. The source must be a clean checkout of commit b050e1cc63b1869e6b514ab2f1a00ef4969e0c84 with no local edits."
    }
}
Remove-Item -Recurse -Force (Join-Path $Out ".git")
Pop-Location

Write-Host "[3/4] Copying the port sources..."
robocopy (Join-Path $repo "overlay") $Out /E /NFL /NDL /NJH /NJS | Out-Null
if ($LASTEXITCODE -ge 8) {
    Write-Error "Failed to copy the Quest overlay (robocopy exit code $LASTEXITCODE)."
}

Write-Host "[4/4] Copying librw with the Vulkan backend..."
robocopy (Join-Path $repo "librw") (Join-Path $Out "vendor\librw") /E /NFL /NDL /NJH /NJS | Out-Null
if ($LASTEXITCODE -ge 8) {
    Write-Error "Failed to copy librw (robocopy exit code $LASTEXITCODE)."
}

Write-Host ""
Write-Host "Done. Next steps:"
Write-Host "  cd $Out\android"
Write-Host "  gradle wrapper   (first time only, or open in Android Studio)"
Write-Host "  .\gradlew assembleDebug"
Write-Host "See BUILDING.md for prerequisites, game data, VR hands and headset installation."
