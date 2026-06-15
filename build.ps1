# Build script for Microphone Indicator (Windows)
# Builds the Rust binary in release mode and packages it using NSIS.

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$sourceExe = Join-Path $scriptDir "target\release\microphone-indicator-windows.exe"
$destDir = Join-Path $scriptDir "releases"
$destExe = Join-Path $destDir "microphone-indicator-windows.exe"
$makensis = Join-Path $scriptDir ".nsis\nsis-3.10\makensis.exe"

# 1. Build the Rust executable
Write-Host "Building Rust project in Release mode..." -ForegroundColor Cyan
cargo build --release

if ($LASTEXITCODE -ne 0) {
    Write-Error "Cargo build failed!"
    exit $LASTEXITCODE
}

# 2. Copy the portable executable to the releases folder
if (-not (Test-Path $destDir)) {
    New-Item -ItemType Directory -Path $destDir | Out-Null
}

Write-Host "Copying executable to $destExe..." -ForegroundColor Green
Copy-Item -Path $sourceExe -Destination $destExe -Force

# 3. Compile the Installer using NSIS
if (Test-Path $makensis) {
    Write-Host "Compiling installer microphone-indicator-setup.exe using NSIS..." -ForegroundColor Cyan
    & $makensis installer.nsi
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Build complete! Clean executable and installer (microphone-indicator-setup.exe) are ready in \releases\ folder." -ForegroundColor Green
    } else {
        Write-Error "NSIS installer compilation failed!"
        exit $LASTEXITCODE
    }
} else {
    Write-Warning "NSIS compiler not found at $makensis. Skipping installer packaging."
}
