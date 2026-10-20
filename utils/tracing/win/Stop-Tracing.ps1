<#
  .SYNOPSIS
  Stops a Trace recording session previously started by running Start-Tracing.ps1, and optionally start analysis.

  .DESCRIPTION
  Stops a Trace recording session previously started by running Start-Tracing.ps1, and optionally start analysis.

  .PARAMETER NoAnalysis
  Include this switch if the script should only stop the session. This can be useful when including this call in other scripts, or to defer analysis to a later time.

  .PARAMETER OutputPath
  None.

  .INPUTS
  None. You cannot pipe objects to this script.

  .OUTPUTS
  None. This script does not generate any output.

  .EXAMPLE
  PS> .\utils\tracing\win\Start-Tracing.ps1 -IncludeGeneralTrace

  .EXAMPLE
  PS> .\utils\tracing\win\Start-Tracing.ps1
#>


[CmdletBinding()]
Param(
    [Parameter(ParameterSetName='Default', Position=0, ValueFromPipeline=$true, ValueFromPipelineByPropertyName=$true)]
    [switch]$NoAnalysis
)

begin {
    Set-StrictMode -Version Latest

    $scriptPath = Split-Path -parent $MyInvocation.MyCommand.Definition
    . (Join-Path -Path $scriptPath -ChildPath "TracingTools.ps1")
    $timeStamp = [DateTime]::Now.ToString("yyyyMMdd_HHmmss")
    $etlFileName = "rnw_" + $timeStamp + ".etl"
    $etlFilePath = Join-Path -Path $scriptPath -ChildPath $etlFileName
    $wprArgs = "-stop", $etlFilePath
}

process{
    $wprPath = Find-TracingTool -Name "wpr.exe"
    if (!$wprPath) {
        throw "
    ################################################################
    $(Get-TracingToolNotFoundMessage -Name "wpr.exe")
    ################################################################"
    }

    Write-Host "$wprPath $wprArgs"
    & Start-Process -Verb RunAs -FilePath $wprPath -ArgumentList $wprArgs -PassThru -Wait

    $wpaPath = Find-TracingTool -Name "wpa.exe"
    if (!$wpaPath) {
        Write-Host (Get-TracingToolNotFoundMessage -Name "wpa.exe")
    }

    if ($NoAnalysis.IsPresent) {
        Write-Host "To analys this trace, install WPT and run: 'wpa $etlFilePath'"
    }
    else {

        if (!$wpaPath) {
            throw "
            ################################################################
            $(Get-TracingToolNotFoundMessage -Name "wpa.exe")
            ################################################################"
        }

        $wpaArgs = $etlFilePath
        Write-Host "$wpaPath $wpaArgs"
        & Start-Process -Verb RunAs -FilePath $wpaPath -ArgumentList $wpaArgs
    }
}
