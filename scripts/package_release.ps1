param(
    [string]$BuildDir = "build",
    [string]$Version = "0.1.0",
    [string]$OutDir = "dist",
    [string]$BuildType = "Release"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$vst3 = Join-Path $BuildDir "vst3/XenakisUPIC_artefacts/$BuildType/VST3/Xenakis UPIC.vst3"
$app = Join-Path $BuildDir "app/XenakisUPICStudio_artefacts/$BuildType/Xenakis UPIC Studio.exe"

if (!(Test-Path $vst3)) { throw "missing $vst3" }
if (!(Test-Path $app)) { throw "missing $app" }

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$out = (Resolve-Path $OutDir).Path
$stage = Join-Path $out "staging"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path (Join-Path $stage "vst3") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $stage "studio") | Out-Null

$vst3Root = Join-Path $stage "vst3"
Copy-Item -Recurse $vst3 (Join-Path $vst3Root "Xenakis UPIC.vst3")
Copy-Item -Recurse (Join-Path $root "presets") $vst3Root
foreach ($file in @("README.md", "NOTICE.md", "LICENSE", "packaging/INSTALL.txt")) {
    Copy-Item (Join-Path $root $file) $vst3Root
}
Copy-Item -Recurse (Join-Path $root "LICENSES") $vst3Root
New-Item -ItemType Directory -Force -Path (Join-Path $vst3Root "docs") | Out-Null
Copy-Item (Join-Path $root "docs/concepts.md") (Join-Path $vst3Root "docs")

$vst3Zip = Join-Path $out "xenakis-upic-$Version-windows-x86_64-vst3.zip"
if (Test-Path $vst3Zip) { Remove-Item $vst3Zip }
Compress-Archive -Path (Join-Path $vst3Root "*") -DestinationPath $vst3Zip

$studioName = "xenakis-upic-studio-$Version"
$studioRoot = Join-Path $stage "studio/$studioName"
New-Item -ItemType Directory -Force -Path $studioRoot | Out-Null
Copy-Item $app (Join-Path $studioRoot "Xenakis UPIC Studio.exe")
Copy-Item -Recurse (Join-Path $root "presets") $studioRoot
foreach ($file in @("README.md", "NOTICE.md", "LICENSE", "packaging/INSTALL.txt")) {
    Copy-Item (Join-Path $root $file) $studioRoot
}
Copy-Item -Recurse (Join-Path $root "LICENSES") $studioRoot
New-Item -ItemType Directory -Force -Path (Join-Path $studioRoot "docs") | Out-Null
Copy-Item (Join-Path $root "docs/concepts.md") (Join-Path $studioRoot "docs")

$studioZip = Join-Path $out "xenakis-upic-studio-$Version-windows-x86_64.zip"
if (Test-Path $studioZip) { Remove-Item $studioZip }
Compress-Archive -Path $studioRoot -DestinationPath $studioZip

$files = Get-ChildItem $out -Filter "xenakis-upic-*.zip"
$hashFile = Join-Path $out "SHA256SUMS"
$lines = foreach ($file in $files) {
    $hash = (Get-FileHash $file.FullName -Algorithm SHA256).Hash.ToLower()
    "$hash  $($file.Name)"
}
Set-Content -Path $hashFile -Value $lines -Encoding ascii

Remove-Item -Recurse -Force $stage
Get-ChildItem $out | Format-Table Name, Length
