# Dot-source before starting diagnostics. Never use Start-Transcript here: its
# Windows PowerShell header includes the process command line (and -Serial).
$script:diagnosticLogEnabled = $false
$script:privateDeviceIds = @()
$script:privateDeviceLabels = @{}

function Get-PrivateDeviceLabel {
    param([string]$Id)
    if ($script:privateDeviceLabels.ContainsKey($Id)) { return Protect-DeviceDiagnostic $script:privateDeviceLabels[$Id] }
    return 'Android device'
}

function Protect-DeviceDiagnostic {
    param([AllowEmptyString()][string]$Text)
    $ids = @(@($script:privateDeviceIds) + @($Serial) | Sort-Object { $_.Length } -Descending)
    foreach ($id in $ids) {
        if (-not [string]::IsNullOrWhiteSpace($id)) {
            $Text = $Text.Replace($id, 'personal Quest 3 device')
        }
    }
    return $Text
}

function Initialize-PrivateDiagnosticLog {
    [IO.File]::WriteAllText($LogPath, "Liberty City VR diagnostic log`r`n", [Text.UTF8Encoding]::new($false))
    $script:diagnosticLogEnabled = $true
}

# Installer status messages go to the console and log only after redaction.
# Native ADB output must be captured through Invoke-PrivateAdb first.
function Write-Host {
    param([object]$Object = '', [ConsoleColor]$ForegroundColor)
    $safe = Protect-DeviceDiagnostic ([string]$Object)
    if ($script:diagnosticLogEnabled) {
        try { [IO.File]::AppendAllText($LogPath, $safe + [Environment]::NewLine) }
        catch {
            $script:diagnosticLogEnabled = $false
            Microsoft.PowerShell.Utility\Write-Host 'Warning: diagnostic logging stopped because the log could not be written.'
        }
    }
    if ($PSBoundParameters.ContainsKey('ForegroundColor')) {
        Microsoft.PowerShell.Utility\Write-Host $safe -ForegroundColor $ForegroundColor
    } else {
        Microsoft.PowerShell.Utility\Write-Host $safe
    }
}

function Invoke-PrivateAdbCapture {
    param([string[]]$Arguments, [switch]$Discovery)
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    # A failed process launch need not update LASTEXITCODE in Windows PowerShell.
    $global:LASTEXITCODE = 1
    try {
        $lines = @(& $script:adb @Arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } catch {
        # Invocation errors can embed the full command and its private arguments.
        $lines = @('ADB could not be started. Check the platform-tools installation.')
        $code = 1
    } finally { $ErrorActionPreference = $savedPreference }
    if ($Discovery) {
        foreach ($line in $lines) {
            if ($line -match '^(\S+)\s+(?:device|offline|unauthorized|recovery|sideload)\b') {
                $script:privateDeviceIds += $Matches[1]
                $id = $Matches[1]
                if ($line -match 'model:(\S+)') {
                    $script:privateDeviceLabels[$id] = $Matches[1].Replace('_', ' ')
                }
            }
        }
        # Discovery is raw local data for selection, never diagnostic output.
        return [pscustomobject]@{ ExitCode = $code; Lines = $lines }
    }
    return [pscustomobject]@{
        ExitCode = $code
        Lines = @($lines | ForEach-Object { Protect-DeviceDiagnostic $_ })
    }
}

function Invoke-PrivateAdb {
    $result = Invoke-PrivateAdbCapture -Arguments @($args | ForEach-Object { $_ })
    $global:LASTEXITCODE = $result.ExitCode
    $result.Lines
}
