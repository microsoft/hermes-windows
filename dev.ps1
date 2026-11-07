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

# The committed .npmrc and .yarnrc.yml files name no registry because Dependabot
# reads them and cannot reach PackageFeedProxy. Route the npm and Yarn restores
# that these commands start through the proxy unless the caller set a registry.
$PackageFeedProxy = 'https://packagefeedproxy.microsoft.io/npm/'
$RegistryVariables = 'NPM_CONFIG_REGISTRY', 'YARN_NPM_REGISTRY_SERVER'

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

# Restores a package's dependencies from its lock file when node_modules is
# missing or older than package.json/package-lock.json. Never rewrites the lock.
function Restore-NpmDependencies([string]$Root, [string]$Description) {
    $stamp = Join-Path $Root 'node_modules\.package-lock.json'
    $packageJson = Join-Path $Root 'package.json'
    $packageLock = Join-Path $Root 'package-lock.json'

    if (-not (Test-Path $stamp) -or
        (Get-Item $packageJson).LastWriteTime -gt (Get-Item $stamp).LastWriteTime -or
        (Get-Item $packageLock).LastWriteTime -gt (Get-Item $stamp).LastWriteTime) {
        Write-Host "Installing $Description dependencies..."
        Push-Location $Root
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

function Ensure-AdoScripts {
    Assert-Node24
    Restore-NpmDependencies (Join-Path $ScriptDir '.ado\scripts') 'build script'
}

function Ensure-ForkSync {
    Assert-Node24
    Restore-NpmDependencies (Join-Path $ScriptDir 'tools\fork-sync') 'fork-sync'
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

# .\dev runs in the caller's PowerShell process, so restore its environment.
$savedRegistries = @{}
foreach ($name in $RegistryVariables) {
    $savedRegistries[$name] = [Environment]::GetEnvironmentVariable($name)
    if (-not $savedRegistries[$name]) {
        [Environment]::SetEnvironmentVariable($name, $PackageFeedProxy)
    }
}

try {
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
            Ensure-AdoScripts
            $lint = Join-Path $ScriptDir '.ado\scripts\npm-source-lint.ts'
            & node $lint @Args
        }

        default {
            Show-Help
        }
    }
} finally {
    foreach ($name in $RegistryVariables) {
        [Environment]::SetEnvironmentVariable($name, $savedRegistries[$name])
    }
}

exit $LASTEXITCODE
