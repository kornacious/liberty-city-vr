param(
    [Parameter(Mandatory=$true)][string]$Sdk,
    [Parameter(Mandatory=$true)][string]$JavaHome,
    [Parameter(Mandatory=$true)][string]$OutDir,
    [string]$CMake='cmake',
    [string]$Ninja='ninja'
)
$ErrorActionPreference='Stop'
$Sdk=[IO.Path]::GetFullPath($Sdk)
$JavaHome=[IO.Path]::GetFullPath($JavaHome)
$OutDir=[IO.Path]::GetFullPath($OutDir)
$ndk=Join-Path $Sdk 'ndk/27.2.12479018'
$tools=Join-Path $Sdk 'build-tools/34.0.0'
$androidJar=Join-Path $Sdk 'platforms/android-35/android.jar'
$env:JAVA_HOME=$JavaHome
$env:PATH=(Join-Path $JavaHome 'bin')+[IO.Path]::PathSeparator+$env:PATH
function Check([string]$step){if($LASTEXITCODE-ne0){throw "$step failed ($LASTEXITCODE)"}}
foreach($dir in @($OutDir,"$OutDir/classes","$OutDir/dex","$OutDir/package/lib/arm64-v8a")){[void][IO.Directory]::CreateDirectory($dir)}
& $CMake -S $PSScriptRoot -B "$OutDir/native" -G Ninja "-DCMAKE_MAKE_PROGRAM=$Ninja" "-DCMAKE_TOOLCHAIN_FILE=$ndk/build/cmake/android.toolchain.cmake" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release
Check 'Configure'
& $CMake --build "$OutDir/native" --parallel 4
Check 'Native compilation'
& (Join-Path $JavaHome 'bin/javac.exe') -source 8 -target 8 -classpath $androidJar -d "$OutDir/classes" (Join-Path $PSScriptRoot 'DepthProbeActivity.java')
Check 'Java compilation'
$classes=@(Get-ChildItem "$OutDir/classes" -Filter '*.class' -Recurse | ForEach-Object FullName)
& (Join-Path $tools 'd8.bat') --min-api 29 --lib $androidJar --output "$OutDir/dex" @classes
Check 'Dex compilation'
Copy-Item "$OutDir/dex/classes.dex" "$OutDir/package/classes.dex"
Copy-Item "$OutDir/native/libdepthprobe.so" "$OutDir/package/lib/arm64-v8a/libdepthprobe.so"
& (Join-Path $tools 'aapt.exe') package -f -M (Join-Path $PSScriptRoot 'AndroidManifest.xml') -I $androidJar -F "$OutDir/unsigned.apk"
Check 'APK manifest'
Push-Location "$OutDir/package"
try { & (Join-Path $tools 'aapt.exe') add "$OutDir/unsigned.apk" classes.dex lib/arm64-v8a/libdepthprobe.so; Check 'APK payload' } finally {Pop-Location}
& (Join-Path $tools 'zipalign.exe') -f -p 4 "$OutDir/unsigned.apk" "$OutDir/aligned.apk"
Check 'Alignment'
$key="$OutDir/diagnostic.keystore"
if(-not(Test-Path -LiteralPath $key)){
    & (Join-Path $JavaHome 'bin/keytool.exe') -genkeypair -keystore $key -storepass android -keypass android -alias diagnostic -dname 'CN=Local Depth Control' -keyalg RSA -validity 3650
    Check 'Local diagnostic signing key'
}
& (Join-Path $tools 'apksigner.bat') sign --ks $key --ks-pass pass:android --key-pass pass:android --out "$OutDir/depth-control.apk" "$OutDir/aligned.apk"
Check 'APK signing'
& (Join-Path $tools 'apksigner.bat') verify "$OutDir/depth-control.apk"
Check 'APK verification'
Write-Output "Diagnostic APK: $OutDir/depth-control.apk"
