# Shared format and cache checks for Liberty City VR Xbox profile.
$XboxBuilderVersion = "0.7.1-xbox-lcv-1"

function Get-XboxHash([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
}

function Assert-XboxImg([string]$Root) {
    $img = Join-Path $Root "models/gta3.img"
    $dir = Join-Path $Root "models/gta3.dir"
    if (-not (Test-Path -LiteralPath $img -PathType Leaf)) { throw "Missing models/gta3.img in $Root" }
    if (-not (Test-Path -LiteralPath $dir -PathType Leaf)) { throw "Missing models/gta3.dir in $Root" }
    $bytes = [IO.File]::ReadAllBytes($dir)
    if (($bytes.Length % 32) -ne 0) { throw "Xbox archive directory length is not a multiple of 32 bytes." }
    $entryCount = [int]($bytes.Length / 32)
    if ($entryCount -le 0) { throw "Xbox archive directory has 0 entries." }
    $length = (Get-Item -LiteralPath $img).Length
    $names = @{}
    $expectedSector = 0L
    for ($i = 0; $i -lt $entryCount; $i++) {
        $offset = $i * 32
        $sector = [int64][BitConverter]::ToUInt32($bytes, $offset)
        $size = [int64][BitConverter]::ToUInt32($bytes, $offset + 4)
        $name = [Text.Encoding]::ASCII.GetString($bytes, $offset + 8, 24).Split([char]0)[0]
        if ($name -cnotmatch '^[a-z0-9_]{1,19}\.(dff|txd)$' -or $names.ContainsKey($name) -or
            $sector -ne $expectedSector -or $size -le 0 -or ($sector + $size) * 2048 -gt $length) {
            throw "Invalid Xbox archive directory entry at index $i : $name (sector $sector, size $size)"
        }
        $names[$name] = $true
        $expectedSector = $sector + $size
    }
    if ($expectedSector * 2048 -ne $length) { throw "Xbox IMG has an invalid trailing size: $length (expected $($expectedSector*2048))" }
}

function Test-XboxOverlay([string]$Root) {
    try {
        if (-not (Test-Path -LiteralPath $Root -PathType Container)) { return $false }
        $infoFile = Join-Path $Root "BUILD_INFO.txt"
        if (-not (Test-Path -LiteralPath $infoFile -PathType Leaf)) { return $false }
        $info = [IO.File]::ReadAllLines($infoFile)
        if ($info -cnotcontains "Profile=XBOX") { return $false }
        if ($info -cnotcontains "BuilderVersion=$XboxBuilderVersion") { return $false }
        $img = Join-Path $Root "models/gta3.img"
        $dir = Join-Path $Root "models/gta3.dir"
        if (-not (Test-Path -LiteralPath $img -PathType Leaf) -or (Get-Item -LiteralPath $img).Length -le 0) { return $false }
        if (-not (Test-Path -LiteralPath $dir -PathType Leaf) -or (Get-Item -LiteralPath $dir).Length -le 0) { return $false }
        Assert-XboxImg $Root
        return $true
    } catch { return $false }
}

function Invoke-XboxTool([string]$Tool, [string[]]$Arguments) {
    $saved = $ErrorActionPreference
    try { $ErrorActionPreference = "Continue"; & $Tool @Arguments; $code = $LASTEXITCODE }
    finally { $ErrorActionPreference = $saved }
    if ($code -ne 0) { throw "Tool failed (exit $code): $Tool" }
}

function Remove-XboxTemporary([string]$Path, [string]$Parent) {
    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path)) { return }
    $resolved = [IO.Path]::GetFullPath($Path)
    $expected = [IO.Path]::GetFullPath($Parent).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($expected, [StringComparison]::OrdinalIgnoreCase) -or
        ([IO.Path]::GetFileName($resolved)) -notmatch '^\.xbox\.(build|source|previous)-[0-9a-f]+$') {
        throw "Refusing an unexpected temporary directory: $resolved"
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force -ErrorAction SilentlyContinue
}
