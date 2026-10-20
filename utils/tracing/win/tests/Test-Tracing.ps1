# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

# Tests that the tracing scripts only launch wpr.exe/wpa.exe from trusted
# install locations. Start-Process is mocked, so nothing is launched or
# elevated. Run with Windows PowerShell 5.1 or PowerShell 7:
#   pwsh -NoProfile -File utils\tracing\win\tests\Test-Tracing.ps1

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$tracingDir = Split-Path -Parent $PSScriptRoot
. (Join-Path -Path $tracingDir -ChildPath 'TracingTools.ps1')

$script:failures = 0

function Assert-True($condition, [string]$message) {
   if ($condition) {
      Write-Host "  PASS $message"
   } else {
      Write-Host "  FAIL $message" -ForegroundColor Red
      $script:failures++
   }
}

function Test-IsTrusted([string]$path) {
   foreach ($name in 'wpr.exe', 'wpa.exe') {
      foreach ($dir in (Get-TracingToolDirectories -Name $name)) {
         if ($path -eq (Join-Path -Path $dir -ChildPath $name)) {
            return $true
         }
      }
   }
   return $false
}

# Decoy executables in a directory placed first on PATH and used as the
# current directory. They must never be selected.
$decoyDir = Join-Path -Path ([IO.Path]::GetTempPath()) `
   -ChildPath ("hermes-tracing-test-" + [Guid]::NewGuid())
New-Item -ItemType Directory -Path $decoyDir | Out-Null
foreach ($name in 'wpr.exe', 'wpa.exe') {
   Set-Content -LiteralPath (Join-Path $decoyDir $name) -Value 'decoy'
}
$savedPath = $env:PATH
$env:PATH = "$decoyDir;$env:PATH"
Push-Location -LiteralPath $decoyDir

$global:TracingTestCalls = New-Object System.Collections.ArrayList
function global:Start-Process {
   Param(
      [string]$FilePath,
      [string[]]$ArgumentList,
      [string]$Verb,
      [switch]$PassThru,
      [switch]$Wait
   )
   [void]$global:TracingTestCalls.Add([pscustomobject]@{
      FilePath = $FilePath
      ArgumentList = $ArgumentList
      Verb = $Verb
      PassThru = $PassThru.IsPresent
      Wait = $Wait.IsPresent
   })
}
if ((Get-Command Start-Process).CommandType -ne 'Function') {
   throw 'Start-Process mock is not in effect; refusing to run.'
}

function Invoke-TracingScript([string]$name, [hashtable]$params = @{}) {
   $global:TracingTestCalls.Clear()
   $scriptFile = Join-Path -Path $tracingDir -ChildPath $name
   try {
      & $scriptFile @params 6>$null | Out-Null
      return $null
   } catch {
      return $_
   }
}

try {
   Write-Host 'Trusted directories'
   foreach ($name in 'wpr.exe', 'wpa.exe') {
      $dirs = @(Get-TracingToolDirectories -Name $name)
      Assert-True ($dirs.Count -gt 0) "$name has trusted directories"
      Assert-True ($dirs -notcontains $decoyDir) "$name ignores PATH and CWD"
      foreach ($dir in $dirs) {
         Assert-True ([IO.Path]::IsPathRooted($dir)) "$dir is absolute"
      }
   }

   Write-Host 'Find-TracingTool'
   $wprPath = Find-TracingTool -Name 'wpr.exe'
   $wpaPath = Find-TracingTool -Name 'wpa.exe'
   foreach ($path in @($wprPath, $wpaPath) | Where-Object { $_ }) {
      Assert-True (Test-IsTrusted $path) "$path is in a trusted directory"
      Assert-True (!$path.StartsWith($decoyDir)) "$path is not the decoy"
   }

   if (!$wprPath) {
      Write-Host 'wpr.exe is not installed; checking the error path only.'
      $err = Invoke-TracingScript 'Start-Tracing.ps1'
      Assert-True ($null -ne $err) 'Start-Tracing fails without wpr.exe'
      Assert-True ($global:TracingTestCalls.Count -eq 0) 'nothing launched'
   } else {
      Write-Host 'Start-Tracing.ps1'
      $err = Invoke-TracingScript 'Start-Tracing.ps1'
      Assert-True ($null -eq $err) "no error ($err)"
      $calls = @($global:TracingTestCalls)
      Assert-True ($calls.Count -eq 1) 'one launch'
      Assert-True ($calls[0].FilePath -eq $wprPath) 'launches trusted wpr.exe'
      Assert-True ($calls[0].Verb -eq 'RunAs') 'elevated'
      Assert-True ($calls[0].Wait -and $calls[0].PassThru) 'waits, passes thru'
      $wprp = Join-Path -Path $tracingDir -ChildPath 'hermes.wprp'
      Assert-True (($calls[0].ArgumentList -join ' ') -eq "-start $wprp") `
         'starts the Hermes profile only'

      Write-Host 'Start-Tracing.ps1 -IncludeGeneralTrace'
      $err = Invoke-TracingScript 'Start-Tracing.ps1' @{IncludeGeneralTrace = $true}
      Assert-True ($null -eq $err) "no error ($err)"
      $calls = @($global:TracingTestCalls)
      Assert-True (($calls[0].ArgumentList -join ' ') -eq `
         "-start $wprp -start GeneralProfile") 'adds GeneralProfile'

      Write-Host 'Stop-Tracing.ps1 -NoAnalysis'
      $err = Invoke-TracingScript 'Stop-Tracing.ps1' @{NoAnalysis = $true}
      Assert-True ($null -eq $err) "no error ($err)"
      $calls = @($global:TracingTestCalls)
      Assert-True ($calls.Count -eq 1) 'launches wpr.exe only'
      Assert-True ($calls[0].FilePath -eq $wprPath) 'launches trusted wpr.exe'
      Assert-True ($calls[0].Verb -eq 'RunAs') 'elevated'
      Assert-True ($calls[0].ArgumentList[0] -eq '-stop') 'stops the trace'
      Assert-True ($calls[0].ArgumentList[1] -like '*.etl') 'writes an ETL'

      Write-Host 'Stop-Tracing.ps1'
      $err = Invoke-TracingScript 'Stop-Tracing.ps1'
      $calls = @($global:TracingTestCalls)
      if ($wpaPath) {
         Assert-True ($null -eq $err) "no error ($err)"
         Assert-True ($calls.Count -eq 2) 'launches wpr.exe then wpa.exe'
         Assert-True ($calls[1].FilePath -eq $wpaPath) 'launches trusted wpa.exe'
         Assert-True ($calls[1].Verb -eq 'RunAs') 'elevated'
         Assert-True ($calls[1].ArgumentList[0] -eq $calls[0].ArgumentList[1]) `
            'opens the recorded ETL'
      } else {
         Write-Host '  wpa.exe is not installed; checking the error path only.'
         Assert-True ($null -ne $err) 'fails without wpa.exe'
         Assert-True ($calls.Count -eq 1) 'launches wpr.exe only'
      }
   }
} finally {
   Remove-Item -Path Function:\Start-Process -ErrorAction SilentlyContinue
   Remove-Variable -Name TracingTestCalls -Scope Global -ErrorAction SilentlyContinue
   Pop-Location
   $env:PATH = $savedPath
   Remove-Item -LiteralPath $decoyDir -Recurse -Force
}

if ($script:failures -gt 0) {
   Write-Host "$script:failures check(s) failed." -ForegroundColor Red
   exit 1
}
Write-Host 'All checks passed.'
