# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

<#
.SYNOPSIS
  Developer task runner for hermes-windows.

.DESCRIPTION
  A single entry point for common development tasks.
  Run without arguments or with -? to see available commands.

.EXAMPLE
  .\dev build --help
    .\dev npm-source-lint
  .\dev fork-sync --dep icu-small
  .\dev fork-sync --dep icu-small --status
#>

param(
    [Parameter(Position = 0)]
    [string]$Command,

    [Parameter(Position = 1, ValueFromRemainingArguments)]
    [string[]]$Args
)

$ErrorActionPreference = 'Stop'
$ScriptDir = $PSScriptRoot

# =============================================================================
# Helpers
# =============================================================================

function Assert-Node24 {
    $node = Get-Command node -CommandType Application -ErrorAction Stop |
        Select-Object -First 1
    $version = & $node.Source --version
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to run Node.js from $($node.Source)."
    }

    $match = [regex]::Match($version, '^v(?<major>\d+)\.')
    if (-not $match.Success -or [int]$match.Groups['major'].Value -lt 24) {
        throw "Node.js 24 or newer is required; found '$version' at $($node.Source)."
    }
}

function Ensure-AdoScripts {
    $root = Join-Path $ScriptDir '.ado\scripts'
    $stamp = Join-Path $root 'node_modules\.package-lock.json'
    $packageJson = Join-Path $root 'package.json'
    $packageLock = Join-Path $root 'package-lock.json'

    if (-not (Test-Path $stamp) -or
        (Get-Item $packageJson).LastWriteTime -gt (Get-Item $stamp).LastWriteTime -or
        (Get-Item $packageLock).LastWriteTime -gt (Get-Item $stamp).LastWriteTime) {
        Write-Host 'Installing build script dependencies...'
        Push-Location $root
        try {
            & npm ci --ignore-scripts --no-audit --no-fund
            if ($LASTEXITCODE -ne 0) {
                throw "npm ci failed with exit code $LASTEXITCODE."
            }
        } finally {
            Pop-Location
        }
    }
}

function Ensure-ForkSync {
    $stamp = Join-Path $ScriptDir 'tools\fork-sync\node_modules\.package-lock.json'
    $pkg = Join-Path $ScriptDir 'tools\fork-sync\package.json'

    if (-not (Test-Path $stamp) -or
        (Get-Item $pkg).LastWriteTime -gt (Get-Item $stamp).LastWriteTime) {
        Write-Host 'Installing fork-sync dependencies...'
        Push-Location (Join-Path $ScriptDir 'tools\fork-sync')
        try { npm install } finally { Pop-Location }
    }
}

function Show-Help {
    Write-Host ''
    Write-Host '  hermes-windows developer tasks'
    Write-Host '  =============================='
    Write-Host ''
    Write-Host '  .\dev build [args]              Build Hermes for Windows'
    Write-Host '  .\dev build-fork-sync           Install fork-sync dependencies'
    Write-Host '  .\dev fork-sync [args]           Run fork-sync tool'
    Write-Host '  .\dev npm-source-lint            Validate npm and Yarn package sources'
    Write-Host ''
    Write-Host '  Examples:'
    Write-Host '    .\dev fork-sync --dep icu-small'
    Write-Host '    .\dev fork-sync --dep icu-small --status'
    Write-Host '    .\dev fork-sync --dep icu-small --continue'
    Write-Host '    .\dev fork-sync --dep icu-small --abort'
    Write-Host '    .\dev fork-sync --help'
    Write-Host '    .\dev build --help'
    Write-Host '    .\dev npm-source-lint'
    Write-Host ''
}

# =============================================================================
# Commands
# =============================================================================

switch ($Command) {
    'build' {
        & node (Join-Path $ScriptDir '.ado\scripts\build.js') @Args
    }

    'build-fork-sync' {
        Ensure-ForkSync
        Write-Host 'fork-sync dependencies installed.'
    }

    'fork-sync' {
        Ensure-ForkSync
        & node (Join-Path $ScriptDir 'tools\fork-sync\sync.ts') @Args
    }

    'npm-source-lint' {
        Assert-Node24
        Ensure-AdoScripts
        & node (Join-Path $ScriptDir '.ado\scripts\npm-source-lint.ts') @Args
    }

    default {
        Show-Help
    }
}

exit $LASTEXITCODE
