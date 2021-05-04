[cmdletbinding(SupportsShouldProcess=$True)]
param(
    [Parameter(Mandatory=$False)]
    [string]$BOOST_VERSION="1.72.0",

    [Parameter(Mandatory=$False)]
    [string]$DOWNLOADDIR="downloads",

    [Parameter(Mandatory=$False)]
    [string]$BOOST_EXTRACTED="boost_src"
)

$NugetURL = 'https://dist.nuget.org/win-x86-commandline/latest/nuget.exe'
Invoke-WebRequest -Uri $NugetURL -OutFile "nuget.exe"
Invoke-Expression ".\nuget.exe sources Enable -Name nuget.org"
Invoke-Expression ".\nuget.exe install boost -Version 1.72.0"