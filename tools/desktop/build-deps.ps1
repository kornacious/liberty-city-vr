param([Parameter(Mandatory=$true)][string]$Cache,[Parameter(Mandatory=$true)][string]$Prefix,[Parameter(Mandatory=$true)][string]$Bash)
$ErrorActionPreference='Stop'
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$install=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$install){throw 'Install Visual Studio Desktop C++ tools.'}
$dev=Join-Path $install 'Common7/Tools/VsDevCmd.bat'
$start=[Diagnostics.ProcessStartInfo]::new($env:ComSpec)
$start.Arguments='/d /s /c ""'+$dev+'" -arch=x64 -host_arch=x64 >nul && set"'
$start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true
$process=[Diagnostics.Process]::Start($start)
$environment=$process.StandardOutput.ReadToEnd().Split("`n");$process.WaitForExit()
if($process.ExitCode){throw 'Visual Studio compiler environment setup failed.'}
foreach($line in $environment){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($matches[1],$matches[2].TrimEnd("`r"),'Process')}}
$env:PATH=(Join-Path $install 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin')+';'+(Join-Path $install 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja')+';'+$env:PATH
$env:CC='cl';$env:CXX='cl'
$script=Join-Path $PSScriptRoot 'build-deps.sh'
& $Bash $script $Cache $Prefix windows
if($LASTEXITCODE){throw "Desktop dependency build failed ($LASTEXITCODE)."}
