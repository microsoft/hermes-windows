# React Native External Dependency Download Script
# This script downloads and sets up the React Native components needed for Hermes inspector

param(
    [string]$CommitHash = "a0ed073650cbdc3e1e08c11d18acf73e0952393a",
    [string]$DownloadUrl = "",
    [switch]$Force
)

$ErrorActionPreference = "Stop"

# If no URL provided, construct from commit hash
if (-not $DownloadUrl) {
    $DownloadUrl = "https://github.com/facebook/react-native/archive/$CommitHash.zip"
}

Write-Host "React Native External Dependency Setup" -ForegroundColor Cyan
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host "Commit Hash: $CommitHash" -ForegroundColor Green
Write-Host "Source URL:  $DownloadUrl" -ForegroundColor Green

# Check if already set up (unless -Force)
if ((Test-Path "packages") -and (-not $Force)) {
    Write-Host "React Native components already exist. Use -Force to overwrite." -ForegroundColor Yellow
    exit 0
}

# Download React Native
Write-Host "Downloading React Native archive..." -ForegroundColor Yellow
try {
    Invoke-WebRequest -Uri $DownloadUrl -OutFile "react-native.zip" -UserAgent "PowerShell"
    Write-Host "Download completed successfully" -ForegroundColor Green
} catch {
    Write-Host "Failed to download: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

# Extract archive
Write-Host "Extracting React Native archive..." -ForegroundColor Yellow
try {
    if (Test-Path "react-native-$CommitHash") {
        Remove-Item "react-native-$CommitHash" -Recurse -Force
    }
    Expand-Archive -Path "react-native.zip" -DestinationPath . -Force
    Write-Host "Extraction completed" -ForegroundColor Green
} catch {
    Write-Host "Failed to extract: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

# Create target directory structure
Write-Host "Setting up directory structure..." -ForegroundColor Yellow
if (Test-Path "packages") {
    Remove-Item "packages" -Recurse -Force
}
New-Item -ItemType Directory -Path "packages\react-native\ReactCommon" -Force | Out-Null

# Copy required ReactCommon components
$SourcePath = "react-native-$CommitHash\packages\react-native\ReactCommon"
$TargetPath = "packages\react-native\ReactCommon"

$ComponentsToCopy = @(
    "hermes",
    "jsinspector-modern",
    "runtimeexecutor\ReactCommon"
)

Write-Host "Copying ReactCommon components..." -ForegroundColor Yellow
foreach ($component in $ComponentsToCopy) {
    $componentSourcePath = Join-Path $SourcePath $component
    $componentTargetPath = Join-Path $TargetPath $component

    if (Test-Path $componentSourcePath) {
        Copy-Item $componentSourcePath -Destination $componentTargetPath -Recurse -Force
        Write-Host "  Copied $component" -ForegroundColor Green
    } else {
        Write-Host "  Component not found: $component" -ForegroundColor Yellow
    }
}

# Apply overrides if they exist
$OverridesPath = (Resolve-Path "..\..\API\hermes_shared\inspector\react-native\overrides").Path
$TargetPath = "packages\react-native\ReactCommon\hermes\inspector-modern\chrome"
if (Test-Path $OverridesPath) {
    Write-Host "Applying overrides..." -ForegroundColor Yellow

    $OverrideFiles = Get-ChildItem $OverridesPath -Recurse -File
    foreach ($file in $OverrideFiles) {
        $relativePath = $file.FullName.Substring($OverridesPath.Length + 1)
        $targetFile = Join-Path $TargetPath $relativePath
        if (Test-Path $targetFile) {
            Copy-Item $file.FullName -Destination $targetFile -Force
            Write-Host "  Applied override: $relativePath" -ForegroundColor Green
        }
    }
}

# Clean up temporary files
Write-Host "Cleaning up temporary files..." -ForegroundColor Yellow
Remove-Item "react-native.zip" -Force -ErrorAction SilentlyContinue
Remove-Item "react-native-$CommitHash" -Recurse -Force -ErrorAction SilentlyContinue

# Validation
Write-Host "Validating setup..." -ForegroundColor Yellow
$RequiredFiles = @(
    "packages\react-native\ReactCommon\hermes\inspector-modern\chrome\HermesRuntimeAgentDelegate.h",
    "packages\react-native\ReactCommon\jsinspector-modern\ReactCdp.h",
    "packages\react-native\ReactCommon\runtimeexecutor\ReactCommon\RuntimeExecutor.h"
)

$AllValid = $true
foreach ($file in $RequiredFiles) {
    if (Test-Path $file) {
        Write-Host "  Found: $file" -ForegroundColor Green
    } else {
        Write-Host "  Missing: $file" -ForegroundColor Red
        $AllValid = $false
    }
}

if ($AllValid) {
    Write-Host "" -ForegroundColor Green
    Write-Host "React Native external dependency setup completed successfully!" -ForegroundColor Green
    Write-Host "=======================================" -ForegroundColor Cyan
    Write-Host "Components copied: $($ComponentsToCopy -join ', ')" -ForegroundColor Cyan
    Write-Host "Ready for CMake configuration" -ForegroundColor Cyan
    Write-Host "See README.md for usage information" -ForegroundColor Cyan
} else {
    Write-Host "Setup completed with errors. Some required files are missing." -ForegroundColor Red
    exit 1
}
