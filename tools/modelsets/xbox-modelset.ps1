# Shared adapter: Windows and Unix use the same pinned builder and validation.
function Get-ModelsetPython {
    foreach ($name in @('python', 'python3', 'py')) {
        $command = Get-Command $name -ErrorAction SilentlyContinue
        if ($command) {
            $saved = $ErrorActionPreference
            try {
                $ErrorActionPreference = 'Continue'
                & $command.Source -c 'import sys; sys.exit(0 if sys.version_info >= (3, 9) else 1)' 2>&1 | Out-Null
                if ($LASTEXITCODE -eq 0) { return $command.Source }
            } catch { } finally { $ErrorActionPreference = $saved }
        }
    }
    throw 'Python 3.9 or later is required. Install Python and add it to PATH.'
}
function Invoke-ModelsetPython {
    param([string[]]$Arguments)
    $python = Get-ModelsetPython
    & $python (Join-Path $PSScriptRoot 'xbox-modelset.py') @Arguments | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { throw 'Asset preparation/validation failed. The previous profile was preserved.' }
}
function Test-XboxOverlay([string]$Root) {
    $python = Get-ModelsetPython
    & $python (Join-Path $PSScriptRoot 'xbox-modelset.py') validate --out $Root --profile xbox
    return $LASTEXITCODE -eq 0
}
