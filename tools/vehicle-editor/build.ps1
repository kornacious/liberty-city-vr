[CmdletBinding()]
param(
    [string]$BuildDir = '',
    [string]$OutputDir = '',
    [string]$CMake = '',
    [string]$PythonRuntime = '',
    [switch]$Package
)
$ErrorActionPreference = 'Stop'
# Windows PowerShell can evaluate parameter defaults before PSScriptRoot is set.
# Resolve script-relative defaults after binding; explicit paths remain supported.
if (-not $BuildDir) { $BuildDir = Join-Path $PSScriptRoot '../../out/vehicle-editor-build' }
if (-not $OutputDir) { $OutputDir = Join-Path $PSScriptRoot '../../out/vehicle-editor' }
if (-not $CMake) {
    $found = Get-Command cmake -ErrorAction SilentlyContinue
    if ($found) { $CMake = $found.Source }
    elseif (Test-Path C:/LCVRBuild/.android-sdk/cmake/3.22.1/bin/cmake.exe) {
        $CMake = 'C:/LCVRBuild/.android-sdk/cmake/3.22.1/bin/cmake.exe'
    } else { throw 'Install CMake 3.22+ or pass -CMake.' }
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw 'Visual Studio C++ tools are required.' }
    [void][IO.Directory]::CreateDirectory([IO.Path]::GetFullPath($BuildDir))
    $envScript = Join-Path ([IO.Path]::GetFullPath($BuildDir)) 'compiler-env.cmd'
    Set-Content -LiteralPath $envScript -Value ('@call "' + $vs + '\VC\Auxiliary\Build\vcvars64.bat" >nul'), '@set'
    $compilerEnvironment = & $env:ComSpec /d /c $envScript
    foreach ($line in $compilerEnvironment) {
        if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
    }
}
$cmakeDir = Split-Path $CMake
$ninja = Join-Path $cmakeDir 'ninja.exe'
$options = @('-S', $PSScriptRoot, '-B', $BuildDir, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release')
if (Test-Path $ninja) { $options += "-DCMAKE_MAKE_PROGRAM=$ninja" }
if ($PythonRuntime) {
    $PythonRuntime = [IO.Path]::GetFullPath($PythonRuntime)
    $runtimePython = Join-Path $PythonRuntime python.exe
    if (-not (Test-Path -LiteralPath $runtimePython)) { throw 'The Python runtime folder must contain python.exe.' }
    $options += "-DVEHICLE_EDITOR_PYTHON_RUNTIME=$PythonRuntime"
    $options += "-DPython3_EXECUTABLE=$runtimePython"
}
& $CMake @options
if ($LASTEXITCODE) { throw 'Editor configuration failed.' }
& $CMake --build $BuildDir --target liberty-vehicle-editor placement-tests placement-runtime-tests placement-process-tests --parallel
if ($LASTEXITCODE) { throw 'Editor compilation failed.' }
& (Join-Path $cmakeDir ctest.exe) --test-dir $BuildDir --output-on-failure
if ($LASTEXITCODE) { throw 'Placement tests failed.' }
& $CMake --install $BuildDir --prefix $OutputDir
if ($LASTEXITCODE) { throw 'Editor installation failed.' }
if ($Package) {
    Push-Location $BuildDir
    try { & (Join-Path $cmakeDir cpack.exe) -G ZIP; if ($LASTEXITCODE) { throw 'Packaging failed.' } }
    finally { Pop-Location }
}
Write-Host "Editor: $OutputDir/liberty-vehicle-editor.exe"
