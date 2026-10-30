param(
    [Parameter(Mandatory = $true)][string]$ShaderTools,
    [Parameter(Mandatory = $true)][string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
$kitRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$shaderRoot = Join-Path $kitRoot 'librw/src/vulkan/shaders'
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $outputRoot) { throw 'Choose a new test output directory.' }
New-Item -ItemType Directory -Path $outputRoot | Out-Null

function Invoke-ShaderTool([string]$Name, [string[]]$ToolArguments) {
    & (Join-Path $ShaderTools "$Name.exe") @ToolArguments
    if ($LASTEXITCODE -ne 0) { throw "$Name failed with exit code $LASTEXITCODE" }
}

function Compile-Shader([string]$Name, [string]$Source, [string[]]$Options = @()) {
    $output = Join-Path $outputRoot "$Name.spv"
    Invoke-ShaderTool 'glslc' (@('--target-env=vulkan1.1', '-O', '-I', $shaderRoot,
        '-o', $output, (Join-Path $shaderRoot $Source)) + $Options)
    Invoke-ShaderTool 'spirv-val' @('--target-env', 'vulkan1.1', $output)
    return $output
}

function Read-Spirv([string]$Path) {
    $result = & (Join-Path $ShaderTools 'spirv-dis.exe') $Path
    if ($LASTEXITCODE -ne 0) { throw 'spirv-dis failed' }
    return $result -join "`n"
}

foreach ($source in @('rw_world.vert', 'rw_skin.vert', 'rw_im3d.vert', 'rw_im2d.vert')) {
    Compile-Shader $source $source | Out-Null
}
$world = Compile-Shader 'world' 'rw_world.frag'
$baseline = Compile-Shader 'world-noeffects' 'rw_world.frag' @('-DRW_WORLD_EFFECTS_RESOURCES=0')
$baselineAssembly = Read-Spirv $baseline
if ($baselineAssembly -match 'DescriptorSet 3|Location [45]\b|OpLoopMerge|OpAtomic') {
    throw 'Baseline module retained effects resources/work before specialization.'
}
if (($baselineAssembly | Select-String 'OpImageSample' -AllMatches).Matches.Count -ne 1) {
    throw 'Baseline module must sample only the material texture.'
}
foreach ($alpha in @(0, 1)) {
    # Keep both alpha-test modes working in the resource-free module.
    $baselineSpecialized = Join-Path $outputRoot "baseline-alpha$alpha.spv"
    Invoke-ShaderTool 'spirv-opt' @('--set-spec-const-default-value', "0:$alpha 1:0",
        '--freeze-spec-const', '--fold-spec-const-op-composite', '-O', '--remove-unused-interface-variables',
        '--eliminate-dead-variables', '--compact-ids', $baseline, '-o', $baselineSpecialized)
    Invoke-ShaderTool 'spirv-val' @('--target-env', 'vulkan1.1', $baselineSpecialized)
    $baselineSpecializedAssembly = Read-Spirv $baselineSpecialized
    if (($alpha -eq 0 -and $baselineSpecializedAssembly -match 'OpKill') -or
        ($alpha -eq 1 -and $baselineSpecializedAssembly -notmatch 'OpKill')) {
        throw "Baseline module changed alpha-test behavior (alpha=$alpha)."
    }
    foreach ($effects in @(0, 1)) {
        $specialized = Join-Path $outputRoot "world-alpha$alpha-effects$effects.spv"
        Invoke-ShaderTool 'spirv-opt' @('--set-spec-const-default-value', "0:$alpha 1:$effects",
            '--freeze-spec-const', '--fold-spec-const-op-composite', '-O', '--remove-unused-interface-variables',
            '--eliminate-dead-variables', '--compact-ids', $world, '-o', $specialized)
        Invoke-ShaderTool 'spirv-val' @('--target-env', 'vulkan1.1', $specialized)
        $assembly = Read-Spirv $specialized
        if ($effects -eq 0) {
            if ($assembly -match 'DescriptorSet 3|Location [45]\b|OpLoopMerge|OpAtomic') {
                throw "Effects OFF retained effects resources/work (alpha=$alpha)."
            }
            if (($assembly | Select-String 'OpImageSample' -AllMatches).Matches.Count -ne 1) {
                throw 'Effects OFF must sample only the material texture.'
            }
        } elseif ($assembly -notmatch 'DescriptorSet 3') {
            throw 'Effects ON lost the reflection resources.'
        }
        if (($alpha -eq 0) -and ($assembly -match 'OpKill')) {
            throw 'Opaque world specialization retained discard.'
        }
    }
}

$baselineDev = Compile-Shader 'world-noeffects-dev' 'rw_world.frag' @(
    '-DRW_WORLD_EFFECTS_RESOURCES=0', '-DMIAMIVR_DEV_TOOLS=1')
if ((Read-Spirv $baselineDev) -match 'DescriptorSet 3|Location [45]\b|OpLoopMerge|OpAtomic') {
    throw 'Development visualizer reintroduced baseline effects resources/work.'
}

$coverageOn = Compile-Shader 'im2d-coverage' 'rw_im2d.frag'
$coverageOff = Compile-Shader 'im2d-no-coverage' 'rw_im2d.frag' @('-DRW_IM2D_COVERAGE=0')
if ((Read-Spirv $coverageOn) -notmatch 'OpAtomicOr') { throw 'Coverage ON lost its mask writes.' }
if ((Read-Spirv $coverageOff) -match 'OpAtomic|StorageBuffer|BufferBlock') {
    throw 'Coverage OFF retained storage-buffer writes.'
}

$state = [IO.File]::ReadAllText((Join-Path $kitRoot 'librw/src/vulkan/vkstate.cpp'))
if ($state -notmatch 'getPipelineForKey\(work\[i\]\.shader, work\[i\]\.topology, work\[i\]\.key\)') {
    throw 'Deferred pipeline compilation does not retain the requested effect variant.'
}
if ($state -notmatch 'if\(\(key & \(uint64\(1\) << 21\)\) != 0\)') {
    throw 'Deferred radar depth state does not use the captured key.'
}
if ($state -notmatch 'const uint64 baselineKey = key & ~effectBits;' -or
    $state -notmatch 'reflectionCoverageComplete = 0;') {
    throw 'Cold effects pipelines must preserve baseline geometry without accepting incomplete coverage history.'
}
Write-Host 'PASS: baseline has no effects interface; both alpha modes and development variant checked; all four world specializations checked; coverage OFF has no atomics.'
