"""Exercise production installer selection and diagnostics with fake ADB only.

No Git, game data, network, build toolchain, or real device is used.
"""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
IDS = ('fixture-primary', 'fixture-second', 'fixture-offline', 'fixture-locked')
INSTALLERS = ('build-and-install', 'install-modern-models', 'install-game-files')


def run(command):
    result = subprocess.run(command, capture_output=True, text=True)
    assert result.returncode == 0, 'Fixture failed:\n' + result.stdout + result.stderr
    for identifier in IDS:
        assert identifier not in result.stdout + result.stderr, 'Console leaked a fixture identifier'


def powershell(shell, work):
    fake = work / 'fake-adb.cmd'
    fake.write_text(r'''
@echo off
if "%~1"=="devices" goto devices
if not "%~1"=="-s" exit /b 99
if not "%~2"=="fixture-primary" exit /b 99
echo fixture-primary Success INSTALL_FAILED_UPDATE_INCOMPATIBLE
1>&2 echo fixture-offline stderr fixture-second fixture-locked
exit /b 7
:devices
if "%DEVICE_TEST_MODE%"=="discovery-error" (
  echo fixture-primary discovery failure
  exit /b 9
)
echo List of devices attached
if not "%DEVICE_TEST_MODE%"=="none" echo fixture-primary device model:Quest_3
if "%DEVICE_TEST_MODE%"=="multi" echo fixture-second device model:Quest_3S
echo fixture-offline offline model:Quest_3
echo fixture-locked unauthorized model:Quest_3
exit /b 0
''', encoding='utf-8', newline='\r\n')
    script = work / 'check.ps1'
    script.write_text(r'''
param([string]$Root, [string]$Work, [string]$Serial)
$ErrorActionPreference = 'Stop'
foreach ($installer in @('build-and-install','install-modern-models','install-game-files')) {
    . (Join-Path $Root 'tools/device-diagnostics.ps1')
    $LogPath = Join-Path $Work ($installer + '.log')
    Initialize-PrivateDiagnosticLog
    $script:adb = Join-Path $Work 'fake-adb.cmd'
    $tokens=$null; $errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $Root "tools/$installer.ps1"),[ref]$tokens,[ref]$errors)
    if ($errors.Count) { throw 'Production syntax error' }
    foreach ($name in @('Select-QuestDevice','Get-AdbArguments','Invoke-NativeCapture','Invoke-AdbCapture','Invoke-AdbChecked','Invoke-Checked')) {
        $node=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name},$true)
        if ($node) { . ([scriptblock]::Create($node.Extent.Text)) }
    }
    foreach ($mode in @('single','multi','none','discovery-error')) {
        $env:DEVICE_TEST_MODE=$mode
        $script:Serial=''
        $NonInteractive=$true
        if ($mode -eq 'single') {
            Select-QuestDevice
            if ($Serial -ne 'fixture-primary') { throw 'Auto selection changed' }
        } else {
            $rejected=$false
            try { Select-QuestDevice } catch { $rejected=$true; Write-Host $_.Exception.Message }
            if (-not $rejected) { throw 'Unsafe selection accepted' }
        }
    }
    $env:DEVICE_TEST_MODE='multi'; $NonInteractive=$false; $script:Serial=''
    function Read-Host { return '2' }
    Select-QuestDevice
    if ($Serial -ne 'fixture-second') { throw 'Numbered selection changed' }
    $script:Serial='fixture-locked'; $NonInteractive=$true
    $rejected=$false
    try { Select-QuestDevice } catch { $rejected=$true; Write-Host $_.Exception.Message }
    if (-not $rejected) { throw 'Unauthorized target accepted' }
    $script:Serial='fixture-primary'
    Select-QuestDevice
    $result=Invoke-PrivateAdbCapture -Arguments @('-s',$Serial,'install','-r','fixture.apk')
    if ($result.ExitCode -ne 7 -or ($result.Lines -join '') -notmatch 'INSTALL_FAILED_UPDATE_INCOMPATIBLE') { throw 'ADB status changed' }
    $result.Lines | ForEach-Object { Write-Host $_ }
    $output=@(Invoke-PrivateAdb @(Get-AdbArguments @('shell','content','query')))
    if ($LASTEXITCODE -ne 7) { throw 'ADB wrapper lost failure code' }
    $output | ForEach-Object { Write-Host $_ }
    $rejected=$false
    try { Invoke-AdbChecked -Arguments @('install','-r','fixture.apk') } catch { $rejected=$true; Write-Host $_.Exception.Message }
    if (-not $rejected) { throw 'Failed ADB command accepted' }
    $script:adb = Join-Path $Work 'missing-adb.exe'
    $global:LASTEXITCODE=0
    $missing=Invoke-PrivateAdbCapture -Arguments @('-s',$Serial,'install','fixture.apk')
    if ($missing.ExitCode -eq 0) { throw 'Missing ADB accepted' }
    $missing.Lines | ForEach-Object { Write-Host $_ }
    # Check header and all status/error paths; don't print private fixture values.
    $log=[IO.File]::ReadAllText($LogPath)
    foreach ($id in @('fixture-primary','fixture-second','fixture-offline','fixture-locked')) {
        if ($log.Contains($id)) { throw 'Diagnostic log leaked an identifier' }
    }
    if ($log -notmatch 'personal Quest 3 device' -or $log -match 'Host Application:') { throw 'Unsafe log format' }
}
Write-Host 'PASS PowerShell installer selection, routing, failures, console and log privacy'
''', encoding='utf-8')
    run([shell, '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', str(script),
         '-Root', str(ROOT), '-Work', str(work), '-Serial', IDS[0]])


def bash_path(path):
    value = path.as_posix()
    if re.match(r'^[A-Za-z]:/', value):
        return '/' + value[0].lower() + value[2:]
    return value


def bash(shell, work):
    # Extract only the production selection/ADB functions, never installer main.
    for name in INSTALLERS:
        source = (ROOT / 'tools' / (name + '.sh')).read_text()
        assert '"$ADB"' not in source, 'Installer bypasses private ADB boundary'
        assert 'echo "Quest: $SERIAL"' not in source, 'Installer displays a raw target'
        definitions = []
        for function in ('select_quest_device', 'adb_capture', 'adb_checked', 'adb_checked_quiet', 'adb_shell'):
            match = re.search(r'^' + function + r'\(\) \{\n.*?^\}', source, re.M | re.S)
            if match:
                definitions.append(match.group())
        fixture = work / (name + '-test.sh')
        fixture.write_text('set -uo pipefail\nsource "' + bash_path(ROOT / 'tools/device-diagnostics.sh') + '"\n' + r'''
die() { echo "$1"; exit 1; }
fake_adb() {
  if [ "$1" = devices ]; then
    printf '%s\n' 'List of devices attached' 'fixture-primary device model:Quest_3' 'fixture-second device model:Quest_3S' 'fixture-offline offline' 'fixture-locked unauthorized'
    return 0
  fi
  [ "$1" = -s ] && [ "$2" = fixture-primary ] || return 99
  echo 'fixture-primary Success INSTALL_FAILED_UPDATE_INCOMPATIBLE'
  echo 'fixture-offline stderr fixture-second fixture-locked' >&2
  return 7
}
ADB=fake_adb; SERIAL=''; NON_INTERACTIVE=0
''' + '\n'.join(definitions) + r'''
select_quest_device <<< 1
[ "$SERIAL" = fixture-primary ] || exit 2
private_adb -s "$SERIAL" install -r fixture.apk
[ "$?" -eq 7 ] || exit 3
SERIAL=fixture-locked
if (select_quest_device); then exit 4; fi
SERIAL=''; NON_INTERACTIVE=1
if (select_quest_device); then exit 5; fi
# Check a quiet path too: stderr must not bypass redaction.
SERIAL=fixture-primary
if declare -F adb_checked_quiet >/dev/null; then
  if (adb_checked_quiet shell test); then exit 6; fi
fi
echo 'PASS Bash installer selection, routing and failure privacy'
''', encoding='utf-8', newline='\n')
        log = work / (name + '-bash.log')
        # Include the same tee boundary as production diagnostics.
        command = 'set -o pipefail; bash "$1" 2>&1 | tee "$2"'
        run([shell, '-c', command, '_', bash_path(fixture), bash_path(log)])
        content = log.read_text()
        assert all(identifier not in content for identifier in IDS), 'Bash log leaked an identifier'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--powershell', action='append', default=[])
    parser.add_argument('--bash')
    args = parser.parse_args()
    assert args.powershell or args.bash, 'Select at least one shell'
    for name in (*INSTALLERS, 'prepare-modern-models'):
        source = (ROOT / 'tools' / (name + '.ps1')).read_text()
        assert 'Start-Transcript' not in source, 'Transcript headers can expose launch arguments'
        assert '& $script:adb' not in source, 'Installer bypasses private ADB boundary'
    scratch = ROOT / '.cache'
    scratch.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='device-diagnostics-', dir=scratch) as directory:
        work = Path(directory)
        assert work.resolve().is_relative_to(scratch.resolve())
        for shell in args.powershell:
            powershell(shell, work)
        if args.bash:
            bash(args.bash, work)
    print('PASS device diagnostics: fake ADB only; no Git, game data or real device access')


if __name__ == '__main__':
    main()
