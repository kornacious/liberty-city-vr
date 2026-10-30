param(
    [Parameter(Mandatory = $true)][string]$ShaderTools,
    [Parameter(Mandatory = $true)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $output) { throw 'Choose a new output directory.' }
New-Item -ItemType Directory -Path $output | Out-Null
$shader = Join-Path $root 'librw/src/vulkan/shaders/rw_postfx.frag'
$gain=[regex]::Match([IO.File]::ReadAllText($shader),'(?ms)^float ColourFilterGain\(.*?^\}')
if(-not $gain.Success){throw 'Production colour gain helper missing.'}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'postfx-colour-gain.inc'),$gain.Value)
foreach ($native in @(0, 1)) {
    $binary = Join-Path $output "postfx-$native.spv"
    & (Join-Path $ShaderTools 'glslc.exe') --target-env=vulkan1.1 -O "-DRW_POSTFX_NATIVE=$native" -o $binary $shader
    if ($LASTEXITCODE -ne 0) { throw 'Postprocess shader compilation failed.' }
    & (Join-Path $ShaderTools 'spirv-val.exe') --target-env vulkan1.1 $binary
    if ($LASTEXITCODE -ne 0) { throw 'Postprocess shader validation failed.' }
    $assembly = (& (Join-Path $ShaderTools 'spirv-dis.exe') $binary) -join "`n"
    if ($LASTEXITCODE -ne 0) { throw 'Postprocess shader disassembly failed.' }
    if ($native -eq 1) {
        if ($assembly -match 'Binding [1-4]\b|OpImageGather|OpImageFetch|OpImageQuery') {
            throw 'Native postprocess retained temporal/upscale resources or operations.'
        }
        $samples = ([regex]::Matches($assembly, 'OpImageSample')).Count
        if ($samples -ne 9) { throw "Native FXAA must retain its centre and eight neighbor samples; found $samples." }
    } elseif ($assembly -notmatch 'Binding 4\b' -or $assembly -notmatch 'OpImageFetch') {
        throw 'General postprocess lost temporal/upscale support.'
    }
}
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -OutDir (Join-Path $output 'host') -Sources @('test-postfx-native.cpp','test-postfx-colour.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Postprocess policy tests failed.' }
Write-Host 'PASS: both postprocess shaders validate; native mode retains FXAA without temporal resources; fallback policy checked.'
