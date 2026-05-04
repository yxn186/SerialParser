$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build_release"

if (Test-Path $BuildDir) {
    Remove-Item -Recurse -Force $BuildDir
}

cmake -S $ProjectRoot -B $BuildDir -G "Ninja" `
    -DCMAKE_PREFIX_PATH="D:/Qt/6.11.0/mingw_64" `
    -DCMAKE_C_COMPILER="D:/Qt/Tools/mingw1310_64/bin/gcc.exe" `
    -DCMAKE_CXX_COMPILER="D:/Qt/Tools/mingw1310_64/bin/g++.exe" `
    -DCMAKE_RC_COMPILER="D:/Qt/Tools/mingw1310_64/bin/windres.exe" `
    -DCMAKE_BUILD_TYPE=Release

cmake --build $BuildDir

Write-Host ""
Write-Host ("Release build completed: " + $BuildDir)
