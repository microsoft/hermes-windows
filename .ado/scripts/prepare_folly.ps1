[cmdletbinding(SupportsShouldProcess=$True)]
param(
    [Parameter(Mandatory=$False)]
    [string]$FOLLY_VERSION="2020.01.13.00",

    [Parameter(Mandatory=$False)]
    [string]$DOWNLOADDIR="downloads",

    [Parameter(Mandatory=$False)]
    [string]$FOLLY_EXTRACTED="folly_src"
)

$FOLLY_URL="https://github.com/facebook/folly/archive/v${FOLLY_VERSION}.tar.gz"

if (!(Test-Path -Path $DOWNLOADDIR)) {
    New-Item -ItemType "directory" -Path $DOWNLOADDIR | Out-Null
}

$FOLLY_DOWNLOADED="$DOWNLOADDIR\v${FOLLY_VERSION}.tar.gz"
Invoke-WebRequest -Uri $FOLLY_URL -OutFile $FOLLY_DOWNLOADED

Invoke-Expression "$PSScriptRoot\Extract-TarGz.ps1 -FileToExtract $FOLLY_DOWNLOADED -TargetFolder $FOLLY_EXTRACTED"