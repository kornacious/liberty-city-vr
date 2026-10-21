param(
    [Parameter(Mandatory=$true)][string]$Apk,
    [switch]$ExpectAsan
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead([IO.Path]::GetFullPath($Apk))
try {
    $runtime = @($archive.Entries | Where-Object FullName -Like 'lib/*/libclang_rt.asan-*-android.so')
    $wrappers = @($archive.Entries | Where-Object FullName -Like 'lib/*/wrap.sh')
    if ($ExpectAsan) {
        if ($runtime.Count -ne 1 -or $runtime[0].FullName -ne 'lib/arm64-v8a/libclang_rt.asan-aarch64-android.so') {
            throw 'Diagnostic APK must contain exactly the arm64 ASan runtime.'
        }
        if ($wrappers.Count -ne 1 -or $wrappers[0].FullName -ne 'lib/arm64-v8a/wrap.sh') {
            throw 'Diagnostic APK must contain the arm64 Android launch wrapper.'
        }
        $reader = [IO.StreamReader]::new($wrappers[0].Open())
        try { $wrapper = $reader.ReadToEnd() } finally { $reader.Dispose() }
        if ($wrapper.Contains("`r") -or -not $wrapper.StartsWith("#!/system/bin/sh`n")) {
            throw 'The Android launch wrapper must use LF line endings and a valid shebang.'
        }
        if ($wrapper -notmatch 'LD_PRELOAD=.*libclang_rt\.asan-aarch64-android\.so .*libc\+\+_shared\.so' -or
            $wrapper -notmatch 'exec "\$@"') {
            throw 'The launch wrapper must preload ASan and the STL before executing the Android process.'
        }
    } elseif ($runtime.Count -or $wrappers.Count) {
        throw 'A player APK must exclude the diagnostic ASan runtime and wrapper.'
    }
    Write-Host "PASS: APK sanitizer packaging (expected ASan=$ExpectAsan): $Apk"
} finally { $archive.Dispose() }
