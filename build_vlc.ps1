# Build/Download VLC SDK for Windows
# This script downloads the VLC SDK and sets it up for m1-player
# Usage: powershell -ExecutionPolicy Bypass -File build_vlc.ps1 -BuildDir <cmake-build-dir>
#
# Example: 
#   powershell -ExecutionPolicy Bypass -File build_vlc.ps1 -BuildDir build-dev
#   powershell -ExecutionPolicy Bypass -File build_vlc.ps1 -BuildDir build

param(
    [Parameter(Mandatory=$true)]
    [string]$BuildDir
)

$ErrorActionPreference = "Stop"

# Configuration
$VLC_VERSION = "3.0.21"
$VLC_SDK_URL = "https://get.videolan.org/vlc/$VLC_VERSION/win64/vlc-$VLC_VERSION-win64.7z"
$VLC_SDK_FALLBACK_URL = "https://mirror.csclub.uwaterloo.ca/vlc/vlc/$VLC_VERSION/win64/vlc-$VLC_VERSION-win64.7z"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$CMakeBuildDir = Join-Path $ScriptDir $BuildDir
$VlcInstallDir = Join-Path $CMakeBuildDir "vlc-install"
$VlcTempDir = Join-Path $CMakeBuildDir "vlc-temp"
$VlcArchive = Join-Path $VlcTempDir "vlc-$VLC_VERSION-win64.7z"

Write-Host "========================================"
Write-Host "VLC SDK Setup for Windows"
Write-Host "========================================"
Write-Host ""
Write-Host "VLC Version: $VLC_VERSION"
Write-Host "Build Dir:   $CMakeBuildDir"
Write-Host "Install Dir: $VlcInstallDir"
Write-Host ""

# Check if CMake build directory exists
if (-not (Test-Path $CMakeBuildDir)) {
    Write-Host "Creating CMake build directory: $CMakeBuildDir"
    New-Item -ItemType Directory -Path $CMakeBuildDir -Force | Out-Null
}

# Check if already installed
if ((Test-Path (Join-Path $VlcInstallDir "lib\libvlc.lib")) -and 
    (Test-Path (Join-Path $VlcInstallDir "include\vlc\vlc.h"))) {
    Write-Host "VLC SDK already installed at: $VlcInstallDir"
    Write-Host ""
    $response = Read-Host "Reinstall? (y/N)"
    if ($response -notmatch "^[Yy]$") {
        Write-Host "Using existing installation."
        exit 0
    }
    Write-Host "Removing existing installation..."
    Remove-Item -Recurse -Force $VlcInstallDir -ErrorAction SilentlyContinue
}

# Create temp directory
if (-not (Test-Path $VlcTempDir)) {
    New-Item -ItemType Directory -Path $VlcTempDir -Force | Out-Null
}

# Download VLC
if (-not (Test-Path $VlcArchive)) {
    Write-Host "Downloading VLC $VLC_VERSION..."
    Write-Host "  URL: $VLC_SDK_URL"
    
    try {
        # Use TLS 1.2
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        
        # Try primary URL
        $webClient = New-Object System.Net.WebClient
        $webClient.DownloadFile($VLC_SDK_URL, $VlcArchive)
    }
    catch {
        Write-Host "  Primary download failed, trying fallback..."
        Write-Host "  URL: $VLC_SDK_FALLBACK_URL"
        try {
            $webClient = New-Object System.Net.WebClient
            $webClient.DownloadFile($VLC_SDK_FALLBACK_URL, $VlcArchive)
        }
        catch {
            Write-Host "Error: Failed to download VLC SDK"
            Write-Host $_.Exception.Message
            exit 1
        }
    }
    Write-Host "  Download complete."
} else {
    Write-Host "Using cached archive: $VlcArchive"
}

# Extract VLC
Write-Host ""
Write-Host "Extracting VLC SDK..."

# Check for 7-Zip
$sevenZipPaths = @(
    "C:\Program Files\7-Zip\7z.exe",
    "C:\Program Files (x86)\7-Zip\7z.exe",
    "$env:PROGRAMFILES\7-Zip\7z.exe"
)

$sevenZip = $null
foreach ($path in $sevenZipPaths) {
    if (Test-Path $path) {
        $sevenZip = $path
        break
    }
}

if (-not $sevenZip) {
    # Try to find 7z in PATH
    $sevenZip = Get-Command "7z" -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
}

if (-not $sevenZip) {
    Write-Host ""
    Write-Host "Error: 7-Zip not found!"
    Write-Host ""
    Write-Host "Please install 7-Zip from: https://www.7-zip.org/"
    Write-Host "Or install via winget: winget install 7zip.7zip"
    Write-Host "Or install via chocolatey: choco install 7zip"
    Write-Host ""
    exit 1
}

Write-Host "  Using 7-Zip: $sevenZip"

# Extract to temp directory
$extractDir = Join-Path $VlcTempDir "extracted"
if (Test-Path $extractDir) {
    Remove-Item -Recurse -Force $extractDir
}

& $sevenZip x $VlcArchive -o"$extractDir" -y | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host "Error: Failed to extract VLC archive"
    exit 1
}

# Find the extracted VLC directory
$vlcExtracted = Get-ChildItem -Path $extractDir -Directory | Select-Object -First 1
if (-not $vlcExtracted) {
    Write-Host "Error: Could not find extracted VLC directory"
    exit 1
}

$vlcSourceDir = $vlcExtracted.FullName
Write-Host "  Extracted to: $vlcSourceDir"

# Create install directory structure
Write-Host ""
Write-Host "Setting up VLC SDK..."

New-Item -ItemType Directory -Path (Join-Path $VlcInstallDir "bin") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $VlcInstallDir "lib") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $VlcInstallDir "include") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $VlcInstallDir "lib\vlc\plugins") -Force | Out-Null

# Copy SDK files
Write-Host "  Copying SDK headers..."
Copy-Item -Path (Join-Path $vlcSourceDir "sdk\include\*") -Destination (Join-Path $VlcInstallDir "include") -Recurse -Force

Write-Host "  Copying SDK libraries..."
Copy-Item -Path (Join-Path $vlcSourceDir "sdk\lib\*") -Destination (Join-Path $VlcInstallDir "lib") -Recurse -Force

Write-Host "  Copying runtime DLLs..."
Copy-Item -Path (Join-Path $vlcSourceDir "libvlc.dll") -Destination (Join-Path $VlcInstallDir "bin") -Force
Copy-Item -Path (Join-Path $vlcSourceDir "libvlccore.dll") -Destination (Join-Path $VlcInstallDir "bin") -Force

Write-Host "  Copying plugins..."
Copy-Item -Path (Join-Path $vlcSourceDir "plugins\*") -Destination (Join-Path $VlcInstallDir "lib\vlc\plugins") -Recurse -Force

# Copy additional DLLs that VLC needs
Write-Host "  Copying additional dependencies..."
$additionalDlls = Get-ChildItem -Path $vlcSourceDir -Filter "*.dll" | Where-Object { $_.Name -notlike "libvlc*.dll" }
foreach ($dll in $additionalDlls) {
    Copy-Item -Path $dll.FullName -Destination (Join-Path $VlcInstallDir "bin") -Force
}

# Cleanup temp files
Write-Host ""
Write-Host "Cleaning up..."
Remove-Item -Recurse -Force $extractDir -ErrorAction SilentlyContinue

# Verify installation
Write-Host ""
Write-Host "========================================"
Write-Host "VLC SDK Setup Complete!"
Write-Host "========================================"
Write-Host ""

$libvlcLib = Join-Path $VlcInstallDir "lib\libvlc.lib"
$libvlccoreLib = Join-Path $VlcInstallDir "lib\libvlccore.lib"
$vlcHeader = Join-Path $VlcInstallDir "include\vlc\vlc.h"
$libvlcDll = Join-Path $VlcInstallDir "bin\libvlc.dll"

if ((Test-Path $libvlcLib) -and (Test-Path $libvlccoreLib) -and (Test-Path $vlcHeader) -and (Test-Path $libvlcDll)) {
    Write-Host "Libraries:"
    Write-Host "  $libvlcLib"
    Write-Host "  $libvlccoreLib"
    Write-Host ""
    Write-Host "Headers:"
    Write-Host "  $(Join-Path $VlcInstallDir "include\vlc")"
    Write-Host ""
    Write-Host "Runtime DLLs:"
    Write-Host "  $(Join-Path $VlcInstallDir "bin")"
    Write-Host ""
    Write-Host "Plugins:"
    Write-Host "  $(Join-Path $VlcInstallDir "lib\vlc\plugins")"
    Write-Host ""
    Write-Host "Next steps:"
    Write-Host "  1. Reconfigure CMake: make dev-player (or make configure)"
    Write-Host "  2. Build m1-player: cmake --build m1-player/$BuildDir --config Release"
    Write-Host ""
} else {
    Write-Host "Error: VLC SDK installation verification failed!"
    Write-Host "Missing files - please check the installation manually."
    exit 1
}



