# Shared adapter: Windows and Unix use the same pinned builder and validation.
function Get-ModelsetPython {
    foreach ($name in @('python', 'python3', 'py')) {
        $command = Get-Command $name -ErrorAction SilentlyContinue
        if ($command) {
            Write-Host "Checking Python 3.9+ ($name)..."
            $process = $null
            try {
                $info = New-Object System.Diagnostics.ProcessStartInfo
                $info.FileName = $command.Source
                $info.Arguments = '-c "import sys; sys.exit(0 if sys.version_info >= (3, 9) else 1)"'
                $info.UseShellExecute = $false
                $info.CreateNoWindow = $true
                $info.RedirectStandardOutput = $true
                $info.RedirectStandardError = $true
                $process = [System.Diagnostics.Process]::Start($info)
                if (-not $process.WaitForExit(5000)) {
                    $process.Kill()
                    Write-Host "$name did not respond within 5 seconds; trying the next launcher."
                    continue
                }
                if ($process.ExitCode -eq 0) { return $command.Source }
            } catch { } finally {
                if ($null -ne $process) { $process.Dispose() }
            }
        }
    }
    throw 'Python 3.9 or later is required. Install Python and add it to PATH.'
}
function Invoke-ModelsetPython {
    param([string[]]$Arguments)
    $python = Get-ModelsetPython
    & $python -u (Join-Path $PSScriptRoot 'xbox-modelset.py') @Arguments | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { throw 'Asset preparation/validation failed. The previous profile was preserved.' }
}
function Test-XboxOverlay([string]$Root) {
    $python = Get-ModelsetPython
    & $python (Join-Path $PSScriptRoot 'xbox-modelset.py') validate --out $Root --profile xbox
    return $LASTEXITCODE -eq 0
}
