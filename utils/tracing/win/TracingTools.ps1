# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

# Locates the Windows Performance Toolkit executables that the tracing scripts
# run with administrator rights. Only fixed, administrator-protected install
# locations are searched. PATH and the current directory are deliberately
# ignored so that a planted wpr.exe or wpa.exe can never be elevated.

<#
  .SYNOPSIS
  Returns the trusted directories searched for a tracing tool, in order.

  .PARAMETER Name
  The tool file name: wpr.exe or wpa.exe.
#>
function Get-TracingToolDirectories {
   [CmdletBinding()]
   Param(
      [Parameter(Mandatory=$true)]
      [ValidateSet('wpr.exe', 'wpa.exe')]
      [string]$Name
   )

   # Known folders are used instead of environment variables, which the
   # current user can change.
   $directories = @()
   if ($Name -eq 'wpr.exe') {
      # wpr.exe ships with Windows 10 and later.
      $directories += [Environment]::SystemDirectory
   }

   $wptSubPath = 'Windows Kits\10\Windows Performance Toolkit'
   $programFiles = @(
      [Environment]::GetFolderPath('ProgramFilesX86'),
      [Environment]::GetFolderPath('ProgramFiles')
   ) | Where-Object { $_ } | Select-Object -Unique
   foreach ($root in $programFiles) {
      $directories += Join-Path -Path $root -ChildPath $wptSubPath
   }

   return $directories
}

<#
  .SYNOPSIS
  Returns the full path of a tracing tool found in a trusted directory, or
  $null if it is not installed there.

  .PARAMETER Name
  The tool file name: wpr.exe or wpa.exe.
#>
function Find-TracingTool {
   [CmdletBinding()]
   Param(
      [Parameter(Mandatory=$true)]
      [ValidateSet('wpr.exe', 'wpa.exe')]
      [string]$Name
   )

   foreach ($directory in (Get-TracingToolDirectories -Name $Name)) {
      $path = Join-Path -Path $directory -ChildPath $Name
      if (Test-Path -LiteralPath $path -PathType Leaf) {
         return $path
      }
   }
   return $null
}

<#
  .SYNOPSIS
  Returns the message shown when a tracing tool is not installed.

  .PARAMETER Name
  The tool file name: wpr.exe or wpa.exe.
#>
function Get-TracingToolNotFoundMessage {
   [CmdletBinding()]
   Param(
      [Parameter(Mandatory=$true)]
      [ValidateSet('wpr.exe', 'wpa.exe')]
      [string]$Name
   )

   $description = @{
      'wpr.exe' = 'Windows Performance Recorder'
      'wpa.exe' = 'Windows Performance Analyzer'
   }[$Name]
   $searched = (Get-TracingToolDirectories -Name $Name) -join "`n      "
   return "$Name ($description) unavailable !
   Install WPT (Windows Performance Toolkit) from the Windows SDK or ADK.
   For security, only these locations are searched (PATH is ignored):
      $searched"
}
