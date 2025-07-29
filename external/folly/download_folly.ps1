# Folly External Dependency Download Script
# This script downloads and sets up the Folly library needed for Hermes inspector

param(
    [string]$Version = "v2020.01.13.00",
    [string]$DownloadUrl = "",
    [switch]$Force
)

$ErrorActionPreference = "Stop"

# If no URL provided, construct from version
if (-not $DownloadUrl) {
    $DownloadUrl = "https://github.com/facebook/folly/archive/$Version.zip"
}

Write-Host "Folly External Dependency Setup" -ForegroundColor Cyan
Write-Host "===============================" -ForegroundColor Cyan
Write-Host "Version: $Version" -ForegroundColor Green
Write-Host "Source URL: $DownloadUrl" -ForegroundColor Green

# Check if already set up (unless -Force)
if ((Test-Path "src") -and (-not $Force)) {
    Write-Host "Folly components already exist. Use -Force to overwrite." -ForegroundColor Yellow
    exit 0
}

# Download Folly
Write-Host "Downloading Folly archive..." -ForegroundColor Yellow
try {
    Invoke-WebRequest -Uri $DownloadUrl -OutFile "folly.zip" -UserAgent "PowerShell"
    Write-Host "Download completed successfully" -ForegroundColor Green
} catch {
    Write-Host "Failed to download: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

# Extract archive
$ExtractDir = "folly-" + $Version.TrimStart('v')
Write-Host "Extracting Folly archive to $ExtractDir..." -ForegroundColor Yellow
try {
    if (Test-Path $ExtractDir) {
        Remove-Item $ExtractDir -Recurse -Force
    }
    Expand-Archive -Path "folly.zip" -DestinationPath . -Force
    Write-Host "Extraction completed" -ForegroundColor Green
} catch {
    Write-Host "Failed to extract: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

# Copy only the specific Folly files we actually need
Write-Host "Setting up minimal Folly source..." -ForegroundColor Yellow
$ExtractedPath = (Resolve-Path $ExtractDir).Path
if (Test-Path "src") {
    Remove-Item "src" -Recurse -Force
}

# Create the folly directory structure
New-Item -ItemType Directory -Path "src\folly" -Force | Out-Null

# List of specific files we need (based on API\hermes_shared\inspector\CMakeLists.txt)
$RequiredFiles = @(
    "folly\concurrency\CacheLocality.cpp",
    "folly\Conv.cpp",
    "folly\Demangle.cpp",
    "folly\detail\AsyncTrace.cpp",
    "folly\detail\AtFork.cpp",
    "folly\detail\Futex.cpp",
    "folly\detail\MemoryIdler.cpp",
    "folly\detail\StaticSingletonManager.cpp",
    "folly\detail\ThreadLocalDetail.cpp",
    "folly\detail\UniqueInstance.cpp",
    "folly\dynamic.cpp",
    "folly\ExceptionWrapper.cpp",
    "folly\Executor.cpp",
    "folly\executors\InlineExecutor.cpp",
    "folly\executors\QueuedImmediateExecutor.cpp",
    "folly\Format.cpp",
    "folly\hash\SpookyHashV2.cpp",
    "folly\io\async\Request.cpp",
    "folly\json_pointer.cpp",
    "folly\json.cpp",
    "folly\lang\Assume.cpp",
    "folly\lang\CString.cpp",
    "folly\lang\SafeAssert.cpp",
    "folly\memory\detail\MallocImpl.cpp",
    "folly\memory\MallctlHelper.cpp",
    "folly\portability\SysMembarrier.cpp",
    "folly\ScopeGuard.cpp",
    "folly\SharedMutex.cpp",
    "folly\String.cpp",
    "folly\synchronization\AsymmetricMemoryBarrier.cpp",
    "folly\synchronization\Hazptr.cpp",
    "folly\synchronization\ParkingLot.cpp",
    "folly\synchronization\SanitizeThread.cpp",
    "folly\Unicode.cpp"
)

# Essential header directories and files
$RequiredHeaders = @(
    "folly\*.h",
    "folly\concurrency\*.h",
    "folly\container\*.h",
    "folly\container\detail\*.h",
    "folly\detail\*.h", 
    "folly\executors\*.h",
    "folly\experimental\*.h",
    "folly\functional\*.h",
    "folly\futures\*.h",
    "folly\futures\detail\*.h",
    "folly\hash\*.h",
    "folly\io\*.h",
    "folly\io\async\*.h",
    "folly\lang\*.h",
    "folly\memory\*.h",
    "folly\memory\detail\*.h",
    "folly\net\*.h",
    "folly\net\detail\*.h",
    "folly\portability\*.h",
    "folly\synchronization\*.h",
    "folly\synchronization\detail\*.h",
    "folly\system\*.h",
    "folly\tracing\*.h"
)

Write-Host "Copying required source files..." -ForegroundColor Yellow
$CopiedCount = 0
foreach ($file in $RequiredFiles) {
    $fileSourcePath = Join-Path $ExtractedPath $file
    $fileTargetPath = Join-Path "src" $file

    if (Test-Path $fileSourcePath) {
        # Create target directory if it doesn't exist
        $targetDir = Split-Path $fileTargetPath -Parent
        if (-not (Test-Path $targetDir)) {
            New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
        }

        Copy-Item $fileSourcePath -Destination $fileTargetPath -Force
        Write-Host "  Copied: $file" -ForegroundColor Green
        $CopiedCount++
    } else {
        Write-Host "  Missing: $file" -ForegroundColor Yellow
    }
}

Write-Host "Copying required header files..." -ForegroundColor Yellow
foreach ($headerPattern in $RequiredHeaders) {
    $sourcePattern = Join-Path $ExtractedPath $headerPattern
    $headers = Get-ChildItem $sourcePattern -File -ErrorAction SilentlyContinue
    
    foreach ($header in $headers) {
        $relativePath = $header.FullName.Substring($ExtractedPath.Length + 1)
        $targetPath = Join-Path "src" $relativePath
        $targetDir = Split-Path $targetPath -Parent
        
        if (-not (Test-Path $targetDir)) {
            New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
        }
        
        Copy-Item $header.FullName -Destination $targetPath -Force
        Write-Host "  Copied: $relativePath" -ForegroundColor Green
        $CopiedCount++
    }
}

Write-Host "Copied $CopiedCount files" -ForegroundColor Green

# Clean up temporary files
Write-Host "Cleaning up temporary files..." -ForegroundColor Yellow
Remove-Item "folly.zip" -Force -ErrorAction SilentlyContinue
Remove-Item $ExtractedPath -Recurse -Force -ErrorAction SilentlyContinue

# Validation
Write-Host "Validating setup..." -ForegroundColor Yellow
$RequiredValidationFiles = @(
    "src\folly\Conv.cpp",
    "src\folly\dynamic.cpp",
    "src\folly\portability\Config.h"
)

$AllValid = $true
foreach ($file in $RequiredValidationFiles) {
    if (Test-Path $file) {
        Write-Host "  Found: $file" -ForegroundColor Green
    } else {
        Write-Host "  Missing: $file" -ForegroundColor Red
        $AllValid = $false
    }
}

if ($AllValid) {
    Write-Host "" -ForegroundColor Green
    Write-Host "Folly external dependency setup completed successfully!" -ForegroundColor Green
    Write-Host "===============================" -ForegroundColor Cyan
    Write-Host "Version: $Version" -ForegroundColor Cyan
    Write-Host "Ready for CMake configuration" -ForegroundColor Cyan
    Write-Host "MSVC patches will be applied automatically" -ForegroundColor Cyan
    Write-Host "See README.md for usage information" -ForegroundColor Cyan
} else {
    Write-Host "Setup completed with errors. Some required files are missing." -ForegroundColor Red
    exit 1
}
