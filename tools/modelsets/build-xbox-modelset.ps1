# Builds a complete PC/Quest-compatible Xbox assets overlay for Liberty City VR
# (Vehicles, characters and pedestrians, weapons, and world/environment).
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Source,
    [Parameter(Mandatory=$true)][string]$Out,
    [switch]$Force
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version 2.0
. (Join-Path $PSScriptRoot "xbox-modelset.ps1")

$sourceRoot = (Resolve-Path -LiteralPath $Source).Path.TrimEnd('\', '/')
$outRoot = [IO.Path]::GetFullPath($Out).TrimEnd('\', '/')

if ([IO.Path]::GetFileName($outRoot) -cne "xbox") {
    throw "Use a separate output folder named 'xbox' (normally game/modelsets/xbox)."
}

foreach ($pair in @(@($sourceRoot, $outRoot), @($outRoot, $sourceRoot))) {
    if (($pair[0] + [IO.Path]::DirectorySeparatorChar).StartsWith($pair[1] + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "The Xbox output and source folders must not overlap."
    }
}

if (Test-Path -LiteralPath $outRoot) {
    if (-not $Force) { throw "Output exists; use -Force for replacement: $outRoot" }
    $info = Join-Path $outRoot "BUILD_INFO.txt"
    if (-not (Test-Path -LiteralPath $info -PathType Leaf) -or [IO.File]::ReadAllLines($info) -cnotcontains "Profile=XBOX") {
        throw "Refusing to replace a folder that is not a generated Xbox profile: $outRoot"
    }
}

$compressor = Join-Path $PSScriptRoot "txdcompress.exe"
if (-not (Test-Path -LiteralPath $compressor -PathType Leaf)) {
    throw "Bundled texture compressor is missing: $compressor"
}

$parent = Split-Path -Parent $outRoot
New-Item -ItemType Directory -Force -Path $parent | Out-Null
$id = [Guid]::NewGuid().ToString("N")
$stage = Join-Path $parent ".xbox.build-$id"
$work = Join-Path $parent ".xbox.source-$id"
$backup = Join-Path $parent ".xbox.previous-$id"
$committed = $false

try {
    New-Item -ItemType Directory -Path $stage, $work, (Join-Path $stage "models/coll"), (Join-Path $stage "models/generic") -Force | Out-Null

    # If the source contains a pre-built gta3.img/dir, extract entries or copy
    $sourceImg = Join-Path $sourceRoot "models/gta3.img"
    if (-not (Test-Path -LiteralPath $sourceImg)) { $sourceImg = Join-Path $sourceRoot "gta3.img" }
    if (Test-Path -LiteralPath $sourceImg -PathType Leaf) {
        $sourceDir = [IO.Path]::ChangeExtension($sourceImg, ".dir")
        if (Test-Path -LiteralPath $sourceDir -PathType Leaf) {
            Write-Host "Extracting source archive: $sourceImg..." -ForegroundColor Cyan
            & (Join-Path $PSScriptRoot "imgextract.ps1") -Img $sourceImg -Out $work
        }
    }

    # Find all .dff and .txd files anywhere under sourceRoot
    $allFiles = @(Get-ChildItem -LiteralPath $sourceRoot -Recurse -File -Force -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -match '^\.(dff|txd)$' })

    if ($allFiles.Count -eq 0 -and @(Get-ChildItem -LiteralPath $work -File).Count -eq 0) {
        throw "No .dff or .txd model files found in source folder: $sourceRoot"
    }

    # Copy files to work folder with lowercase names
    foreach ($file in $allFiles) {
        $lowerName = $file.Name.ToLowerInvariant()
        # Handle wheels separately if placed in generic
        if ($lowerName -eq "wheels.dff" -or $lowerName -eq "wheels.txd") {
            Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $stage "models/generic/$lowerName") -Force
        }
        Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $work $lowerName) -Force
    }

    # Also look for vehicles.col
    $collFiles = @(Get-ChildItem -LiteralPath $sourceRoot -Recurse -File -Filter "vehicles.col" -ErrorAction SilentlyContinue)
    if ($collFiles.Count -gt 0) {
        Copy-Item -LiteralPath $collFiles[0].FullName -Destination (Join-Path $stage "models/coll/vehicles.col") -Force
    }

    # List of known vehicles, weapons, and peds in GTA III to categorize
    $knownVehicles = @(
        "landstal","idaho","stinger","linerun","peren","sentinel","patriot","firetruk",
        "trash","stretch","manana","infernus","blistac","pony","mule","cheetah",
        "ambulan","fbi","moonbeam","esperant","taxi","kuruma","bobcat","mrwhoop",
        "bfinject","corpse","police","enforcer","securica","banshee","predator","bus",
        "rhino","barracks","train","chopper","dodo","coach","cabbie","stallion",
        "rumpo","rcbandit","bellyup","mrwongs","mafia","yardie","yakuza","diablos",
        "columb","hoods","airtrain","deaddodo","speeder","reefer","panlvant","flatbed",
        "yankee","escape","borgnine","toyz"
    )
    $knownWeapons = @(
        "bat","colt45","uzi","shotgun","ak47","m16","sniper","rocket","flame",
        "grenade","molotov","bomb","camera"
    )

    $stagedFiles = @(Get-ChildItem -LiteralPath $work -File | Sort-Object Name)
    Write-Host "Staged $($stagedFiles.Count) model/texture files for Xbox profile." -ForegroundColor Cyan

    # Compress all staged TXDs for mobile VR performance
    Write-Host "Compressing textures with txdcompress for Quest VRAM optimization..." -ForegroundColor Cyan
    $txdFiles = @(Get-ChildItem -LiteralPath $work -File -Filter "*.txd")
    $genericWheelsTxd = Join-Path $stage "models/generic/wheels.txd"
    if (Test-Path -LiteralPath $genericWheelsTxd) { $txdFiles += (Get-Item -LiteralPath $genericWheelsTxd) }

    foreach ($txd in $txdFiles) {
        Invoke-XboxTool $compressor @($txd.FullName)
    }

    # Write gta3.img and gta3.dir
    Write-Host "Packing overlay models into models/gta3.img and models/gta3.dir..." -ForegroundColor Cyan
    $outModelsDir = Join-Path $stage "models"
    New-Item -ItemType Directory -Path $outModelsDir -Force | Out-Null
    $img = [IO.File]::Create((Join-Path $outModelsDir "gta3.img"))
    $dir = [IO.File]::Create((Join-Path $outModelsDir "gta3.dir"))
    $writer = New-Object IO.BinaryWriter($dir)
    try {
        $sector = 0L
        foreach ($file in $stagedFiles) {
            $bytes = [IO.File]::ReadAllBytes($file.FullName)
            $size = [int][Math]::Ceiling($bytes.Length / 2048.0)
            $writer.Write([uint32]$sector)
            $writer.Write([uint32]$size)
            $name = New-Object byte[] 24
            [Text.Encoding]::ASCII.GetBytes($file.Name).CopyTo($name, 0)
            $writer.Write($name)
            $img.Write($bytes, 0, $bytes.Length)
            $padding = New-Object byte[] ($size * 2048 - $bytes.Length)
            if ($padding.Length -gt 0) {
                $img.Write($padding, 0, $padding.Length)
            }
            $sector += $size
        }
    } finally {
        $writer.Dispose()
        $img.Dispose()
        $dir.Dispose()
    }

    # Categorize models
    $dffModels = @($stagedFiles | Where-Object Extension -eq ".dff" | ForEach-Object { $_.BaseName.ToLowerInvariant() })
    $vehicles = @($dffModels | Where-Object { $knownVehicles -contains $_ })
    $weapons = @($dffModels | Where-Object { $knownWeapons -contains $_ })
    $peds = @($dffModels | Where-Object {
        $_ -match '^(player|cop|swat|fbi|army|medic|fireman|male\d+|female\d+|asuka|salvatore|maria|donald|kenji|misty|ray|phil|catalina|miguel|eight|8ball|luigi|joey|chunky|curly|orient|scum|prostitute|dockworker|tramp|criminal|bodyguard|construction|garbageman)'
    })
    $world = @($dffModels | Where-Object {
        $vehicles -notcontains $_ -and $weapons -notcontains $_ -and $peds -notcontains $_
    })

    $utf8 = New-Object Text.UTF8Encoding($false)
    if ($vehicles.Count -gt 0) {
        [IO.File]::WriteAllText((Join-Path $stage "vehicle_models.txt"), (($vehicles | Sort-Object -Unique) -join "`n") + "`n", $utf8)
    }
    if ($peds.Count -gt 0) {
        [IO.File]::WriteAllText((Join-Path $stage "ped_models.txt"), (($peds | Sort-Object -Unique) -join "`n") + "`n", $utf8)
    }
    if ($weapons.Count -gt 0) {
        [IO.File]::WriteAllText((Join-Path $stage "weapon_models.txt"), (($weapons | Sort-Object -Unique) -join "`n") + "`n", $utf8)
    }

    Assert-XboxImg $stage

    # Build Info
    $lines = @(
        "Liberty City VR Xbox Assets Profile",
        "Profile=XBOX",
        "BuilderVersion=$XboxBuilderVersion",
        "VehicleCount=$($vehicles.Count)",
        "PedCount=$($peds.Count)",
        "WeaponCount=$($weapons.Count)",
        "WorldCount=$($world.Count)",
        "ArchiveEntries=$($stagedFiles.Count)"
    )
    foreach ($item in @("models/gta3.img", "models/gta3.dir")) {
        $lines += "SHA256 $item=$(Get-XboxHash (Join-Path $stage $item))"
    }
    foreach ($extra in @("models/coll/vehicles.col", "models/generic/wheels.dff", "models/generic/wheels.txd", "vehicle_models.txt", "ped_models.txt", "weapon_models.txt")) {
        $extraPath = Join-Path $stage $extra
        if (Test-Path -LiteralPath $extraPath -PathType Leaf) {
            $lines += "SHA256 $extra=$(Get-XboxHash $extraPath)"
        }
    }
    $lines += "Assets are supplied from original Xbox game data / conversion packs; do not redistribute."
    [IO.File]::WriteAllText((Join-Path $stage "BUILD_INFO.txt"), ($lines -join "`n") + "`n", $utf8)

    if (-not (Test-XboxOverlay $stage)) {
        throw "Completed Xbox profile failed validation."
    }

    if (Test-Path -LiteralPath $outRoot) { Move-Item -LiteralPath $outRoot -Destination $backup }
    try {
        Move-Item -LiteralPath $stage -Destination $outRoot
        $committed = $true
    } catch {
        if ((Test-Path -LiteralPath $backup) -and -not (Test-Path -LiteralPath $outRoot)) {
            Move-Item -LiteralPath $backup -Destination $outRoot
        }
        throw
    }
    Remove-XboxTemporary $backup $parent
    Write-Host ""
    Write-Host "XBOX PROFILE BUILT AND VERIFIED: $outRoot" -ForegroundColor Green
    Write-Host "  Vehicles: $($vehicles.Count)" -ForegroundColor Green
    Write-Host "  Characters/Peds: $($peds.Count)" -ForegroundColor Green
    Write-Host "  Weapons: $($weapons.Count)" -ForegroundColor Green
    Write-Host "  World & Map: $($world.Count)" -ForegroundColor Green
    Write-Host "  Total archive entries: $($stagedFiles.Count)" -ForegroundColor Green
} finally {
    Remove-XboxTemporary $work $parent
    if (-not $committed) { Remove-XboxTemporary $stage $parent }
}
