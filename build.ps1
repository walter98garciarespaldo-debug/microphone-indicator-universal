# Build script for Microphone Indicator (Windows)
# Builds the Rust binary in release mode and copies it cleanly to the \releases\ directory.

Write-Host "Building Rust project in Release mode..." -ForegroundColor Cyan
cargo build --release

if ($LASTEXITCODE -ne 0) {
    Write-Error "Cargo build failed!"
    exit $LASTEXITCODE
}

$sourceExe = "target\release\microphone-indicator-windows.exe"
$destDir = "releases"
$destExe = "$destDir\microphone-indicator-windows.exe"

if (-not (Test-Path $destDir)) {
    New-Item -ItemType Directory -Path $destDir | Out-Null
}

Write-Host "Copying executable to $destExe..." -ForegroundColor Green
Copy-Item -Path $sourceExe -Destination $destExe -Force

Write-Host "Build complete! Clean executable ready in \releases\ folder." -ForegroundColor Green
