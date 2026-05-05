$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build_release"
$AppDir = Join-Path $BuildDir "app"
$DistDir = Join-Path $ProjectRoot "dist"
$ZipPath = Join-Path $DistDir "SerialParser-Windows-x64.zip"
$LauncherPath = Join-Path $BuildDir "SerialParser.exe"
$AppExePath = Join-Path $BuildDir "SerialParserApp.exe"
$QtBin = "D:/Qt/6.11.0/mingw_64/bin"
$MingwBin = "D:/Qt/Tools/mingw1310_64/bin"
$WinDeployQt = "D:/Qt/6.11.0/mingw_64/bin/windeployqt.exe"

if (!(Test-Path $LauncherPath)) {
    throw "SerialParser.exe launcher was not found. Run scripts/build_release.ps1 first."
}

if (!(Test-Path $AppExePath)) {
    throw "SerialParserApp.exe was not found. Run scripts/build_release.ps1 first."
}

if (!(Test-Path $WinDeployQt)) {
    throw ("windeployqt.exe was not found: " + $WinDeployQt)
}

$env:PATH = $QtBin + ";" + $MingwBin + ";" + $env:PATH

& $WinDeployQt --release --compiler-runtime $AppExePath

Copy-Item -Recurse -Force (Join-Path $ProjectRoot "configs") (Join-Path $BuildDir "configs")
Copy-Item -Recurse -Force (Join-Path $ProjectRoot "styles") (Join-Path $BuildDir "styles")
Copy-Item -Recurse -Force (Join-Path $ProjectRoot "resources") (Join-Path $BuildDir "resources")
Copy-Item -Force (Join-Path $ProjectRoot "README.md") (Join-Path $BuildDir "README.md")

if (Test-Path $AppDir) {
    Remove-Item -Recurse -Force $AppDir
}
New-Item -ItemType Directory -Force -Path $AppDir | Out-Null

$RuntimeDirs = @(
    ".qt",
    "configs",
    "styles",
    "resources",
    "generic",
    "iconengines",
    "imageformats",
    "networkinformation",
    "platforms",
    "tls",
    "translations"
)

foreach ($DirName in $RuntimeDirs) {
    $Source = Join-Path $BuildDir $DirName
    if (Test-Path $Source) {
        Move-Item -Force $Source (Join-Path $AppDir $DirName)
    }
}

$RuntimeFilePatterns = @(
    "SerialParserApp.exe",
    "Qt6*.dll",
    "libgcc_s_seh-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll",
    "opengl32sw.dll",
    "D3Dcompiler_47.dll"
)

foreach ($Pattern in $RuntimeFilePatterns) {
    Get-ChildItem -Path $BuildDir -File -Filter $Pattern -ErrorAction SilentlyContinue | ForEach-Object {
        Move-Item -Force $_.FullName (Join-Path $AppDir $_.Name)
    }
}

$BuildArtifactDirs = @(
    "CMakeFiles",
    "SerialParser_autogen",
    "SerialParserApp_autogen"
)

foreach ($DirName in $BuildArtifactDirs) {
    $Path = Join-Path $BuildDir $DirName
    if (Test-Path $Path) {
        Remove-Item -Recurse -Force $Path
    }
}

$BuildArtifactFiles = @(
    ".ninja_deps",
    ".ninja_log",
    "build.ninja",
    "CMakeCache.txt",
    "cmake_install.cmake",
    "libSerialParserApp.dll.a",
    "libSerialParser.dll.a"
)

foreach ($FileName in $BuildArtifactFiles) {
    $Path = Join-Path $BuildDir $FileName
    if (Test-Path $Path) {
        Remove-Item -Force $Path
    }
}

$RootReadme = Join-Path $ProjectRoot "README.md"
$ReleaseReadme = Join-Path $BuildDir "README.md"
$AppReadme = Join-Path $AppDir "README.md"
Copy-Item -Force $RootReadme $AppReadme
$ReleaseReadmeText = Get-Content -Path $RootReadme -Raw -Encoding UTF8
$ReleaseReadmeText = $ReleaseReadmeText.Replace("resources/readme_hero.png", "app/resources/readme_hero.png")
$ReleaseReadmeText = $ReleaseReadmeText.Replace("resources/readme_hero_preview.png", "app/resources/readme_hero_preview.png")
[System.IO.File]::WriteAllText($ReleaseReadme, $ReleaseReadmeText, [System.Text.UTF8Encoding]::new($false))

$RootReadmeHash = (Get-FileHash $RootReadme -Algorithm SHA256).Hash
$AppReadmeHash = (Get-FileHash $AppReadme -Algorithm SHA256).Hash
if ($RootReadmeHash -ne $AppReadmeHash) {
    throw "README sync failed. Root README.md must match build_release/app/README.md before packaging."
}
if (!(Select-String -Path $ReleaseReadme -SimpleMatch "app/resources/readme_hero.png" -Quiet)) {
    throw "Release README image path rewrite failed. build_release/README.md must point to app/resources/readme_hero.png."
}

$RequiredItems = @(
    @{ Path = "SerialParser.exe"; Message = "Missing root SerialParser.exe launcher." },
    @{ Path = "README.md"; Message = "Missing root README.md." },
    @{ Path = "app/README.md"; Message = "Missing app/README.md." },
    @{ Path = "app/SerialParserApp.exe"; Message = "Missing app/SerialParserApp.exe." },
    @{ Path = "app/Qt6Charts.dll"; Message = "Missing app/Qt6Charts.dll. The chart view will not run." },
    @{ Path = "app/Qt6SerialPort.dll"; Message = "Missing app/Qt6SerialPort.dll. The SerialPort module will not run." },
    @{ Path = "app/platforms/qwindows.dll"; Message = "Missing app/platforms/qwindows.dll. Qt platform plugin cannot load." },
    @{ Path = "app/configs"; Message = "Missing app/configs directory." },
    @{ Path = "app/styles"; Message = "Missing app/styles directory." },
    @{ Path = "app/resources/app_icon.png"; Message = "Missing app/resources/app_icon.png." },
    @{ Path = "app/resources/readme_hero.png"; Message = "Missing app/resources/readme_hero.png." },
    @{ Path = "app/resources/readme_hero_preview.png"; Message = "Missing app/resources/readme_hero_preview.png." }
)

$HasMissing = $false
foreach ($Item in $RequiredItems) {
    $FullPath = Join-Path $BuildDir $Item.Path
    if (!(Test-Path $FullPath)) {
        Write-Warning $Item.Message
        $HasMissing = $true
    }
}

Write-Host ""
if ($HasMissing) {
    Write-Warning "Deploy check found missing items. Check the windeployqt output above."
} else {
    Write-Host "Deploy check passed."
}

if (!(Test-Path $DistDir)) {
    New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
}

if (Test-Path $ZipPath) {
    Remove-Item -Force $ZipPath
}

Compress-Archive -Path $BuildDir -DestinationPath $ZipPath -Force

Write-Host "Release layout:"
Write-Host "  build_release/SerialParser.exe  launcher, run this file"
Write-Host "  build_release/README.md         user guide"
Write-Host "  build_release/app/              app runtime files"
Write-Host ("Release zip: " + $ZipPath)
Write-Host "For release, copy the whole build_release folder."
Write-Host ("Deploy directory: " + $BuildDir)
