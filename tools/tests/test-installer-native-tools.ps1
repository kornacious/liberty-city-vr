param([string]$OutDir=(Join-Path $PSScriptRoot '../../.cache/installer-native-tests'))
$ErrorActionPreference='Stop'
$root=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
function Import-Function([string]$File,[string]$Name) {
    $tokens=$null; $errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile($File,[ref]$tokens,[ref]$errors)
    if($errors){throw 'Production script parse failed.'}
    $node=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $Name},$true)
    if($null -eq $node){throw "Missing production function: $Name"}
    . ([scriptblock]::Create($node.Extent.Text))
    Set-Item -Path "function:script:$Name" -Value (Get-Item "function:$Name").ScriptBlock
}
Import-Function (Join-Path $root 'tools/build-and-install.ps1') 'Invoke-Checked'
$script:adb=$null
$fake=Join-Path ([IO.Path]::GetFullPath($OutDir)) 'native-warning.cmd'
[IO.File]::WriteAllText($fake,"@echo off`r`necho Warning: SDK processing. XML version warning fixture 1>&2`r`necho native command completed`r`nif `"%~1`"==`"fail`" exit /b 7`r`nexit /b 0`r`n",[Text.Encoding]::ASCII)
Invoke-Checked -FilePath $fake -Arguments @('ok')
if($ErrorActionPreference -ne 'Stop'){throw 'Caller preference was not restored.'}
$failed=$false
try { Invoke-Checked -FilePath $fake -Arguments @('fail') -FailureMessage 'Expected native failure' }
catch { if($_.Exception.Message -notmatch 'exit code 7'){throw};$failed=$true }
if(-not $failed){throw 'Nonzero exit was accepted.'}
$failed=$false
try { Invoke-Checked -FilePath (Join-Path $OutDir 'missing.exe') -Arguments @('ok') }
catch {$failed=$true}
if(-not $failed){throw 'Missing tool was accepted.'}
if($ErrorActionPreference -ne 'Stop'){throw 'Caller preference was not restored after failure.'}

Import-Function (Join-Path $root 'tools/install-modern-models.ps1') 'Find-Adb'
# Isolate discovery from installed tools/devices; these virtual candidates
# deliberately include both the current and legacy default SDK locations.
$AndroidSdk='';$env:ANDROID_HOME='';$env:ANDROID_SDK_ROOT='';$env:LOCALAPPDATA=''
$script:existing=@('C:\LCVRBuild\.android-sdk\platform-tools\adb.exe','C:\VCVRBuild\.android-sdk\platform-tools\adb.exe')
function Test-Path { param([string]$LiteralPath,[string]$PathType) return $script:existing -contains $LiteralPath }
function Resolve-Path { param([string]$LiteralPath) return [pscustomobject]@{Path=$LiteralPath} }
function Get-Command { param([string]$Name,[string]$ErrorAction) return $null }
if((Find-Adb) -ne 'C:\LCVRBuild\.android-sdk\platform-tools\adb.exe'){throw 'Default Liberty City SDK not preferred.'}
$AndroidSdk='C:\explicit-sdk';$script:existing+=Join-Path $AndroidSdk 'platform-tools\adb.exe'
if((Find-Adb) -ne (Join-Path $AndroidSdk 'platform-tools\adb.exe')){throw 'Explicit SDK override was ignored.'}
$AndroidSdk='';$script:existing=@();$failed=$false
try { Find-Adb } catch {$failed=$true}
if(-not $failed){throw 'Missing SDK was accepted.'}
Write-Host 'PASS native stderr warning/exit status/preference restoration and isolated ADB SDK discovery'
