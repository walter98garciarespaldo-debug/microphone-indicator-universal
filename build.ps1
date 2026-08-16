# Build script for Microphone Indicator (Windows)
# Compiles the C++ binary with MSVC and packages it using NSIS.
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$cppSrc = Join-Path $scriptDir "src\main.cpp"
$rcSrc = Join-Path $scriptDir "resources\resources.rc"
$rcDir = Join-Path $scriptDir "resources"
$destDir = Join-Path $scriptDir "releases"
$destExe = Join-Path $destDir "microphone-indicator-windows.exe"
$nsiScript = Join-Path $scriptDir "installer\installer.nsi"
$makensis = Join-Path $scriptDir ".nsis\nsis-3.10\makensis.exe"

if (-not (Test-Path $destDir)) {
    New-Item -ItemType Directory -Path $destDir | Out-Null
}

# 1. Locate and initialize MSVC environment if cl.exe is not in PATH
if (-not (Get-Command "cl.exe" -ErrorAction SilentlyContinue)) {
    $vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvars)) {
        $found = Get-ChildItem -Path "C:\Program Files (x86)\Microsoft Visual Studio", "C:\Program Files\Microsoft Visual Studio" -Recurse -Filter "vcvars64.bat" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) {
            $vcvars = $found.FullName
        }
    }

    if (Test-Path $vcvars) {
        Write-Host "Initializing MSVC environment from $vcvars..." -ForegroundColor Cyan
        $cmd = "`"$vcvars`" && set"
        $envVars = cmd.exe /c $cmd
        foreach ($line in $envVars) {
            if ($line -match "^([^=]+)=(.*)$") {
                [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
            }
        }
    } else {
        Write-Error "Could not find MSVC vcvars64.bat!"
        exit 1
    }
}

# 2. Compile Windows resources
Write-Host "Compiling resources: $rcSrc..." -ForegroundColor Cyan
$resObj = Join-Path $rcDir "resources.res"
& rc.exe /fo "$resObj" /i "$rcDir" "$rcSrc"
if ($LASTEXITCODE -ne 0) {
    Write-Error "Resource compilation failed!"
    exit $LASTEXITCODE
}

# 3. Compile C++ executable
Write-Host "Compiling C++ project in Release mode..." -ForegroundColor Cyan
& cl.exe /std:c++17 /O2 /GL /Gy /W4 /EHsc /utf-8 /nologo `
    /D "NDEBUG" /D "_UNICODE" /D "UNICODE" `
    "$cppSrc" "$resObj" `
    /link /SUBSYSTEM:WINDOWS /LTCG /OPT:REF /OPT:ICF /OUT:"$destExe" `
    user32.lib gdi32.lib shell32.lib ole32.lib advapi32.lib

if ($LASTEXITCODE -ne 0) {
    Write-Error "C++ compilation failed!"
    exit $LASTEXITCODE
}

# Clean temporary compilation artifacts
Remove-Item "$scriptDir\*.obj", "$rcDir\*.res" -ErrorAction SilentlyContinue

$exeSize = (Get-Item $destExe).Length / 1KB
Write-Host "C++ build complete: $destExe ($([math]::Round($exeSize, 2)) KB)" -ForegroundColor Green

# 4. Compile the Installer using NSIS
if (Test-Path $makensis) {
    if (Test-Path $nsiScript) {
        Write-Host "Compiling installer microphone-indicator-setup.exe using NSIS..." -ForegroundColor Cyan
        $installerDir = Split-Path -Parent $nsiScript
        Push-Location $installerDir
        try {
            & $makensis (Split-Path -Leaf $nsiScript)
            if ($LASTEXITCODE -eq 0) {
                Write-Host "Build complete! Clean executable and installer (microphone-indicator-setup.exe) are ready in \releases\ folder." -ForegroundColor Green
            } else {
                Write-Error "NSIS installer compilation failed!"
                exit $LASTEXITCODE
            }
        } finally {
            Pop-Location
        }
    } else {
        Write-Warning "NSIS script not found at $nsiScript."
    }
} else {
    Write-Warning "NSIS compiler not found at $makensis. Skipping installer packaging."
}
