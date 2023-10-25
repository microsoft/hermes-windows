param(
    [string]$SourcesPath = "$PSScriptRoot\..\..",
    [string]$WorkSpacePath = "$SourcesPath\workspace",
    [string]$OutputPath = "$WorkSpacePath\out",
    
    [ValidateSet("x64", "x86", "arm64")]
    [String[]]$Platform = @("x64"),

    [ValidateSet("x64", "x86", "arm64")]
    [String]$ToolsPlatform = @("x86"), # Platform used for building tools when cross compiling
    
    [ValidateSet("debug", "release")]
    [String]$ToolsConfiguration = @("release"), # Platform used for building tools when cross compiling
    
    [ValidateSet("debug", "release")]
    [String[]]$Configuration = @("debug"),
    
    [ValidateSet("win32", "uwp")]
    [String]$AppPlatform = "uwp",

    # e.g. "10.0.17763.0"
    [String]$SDKVersion = "",

    # e.g. "0.0.0-2209.28001-8af7870c" for pre-release or "0.70.2" for release
    [String]$ReleaseVersion = "",

    # e.g. "0.0.2209.28001" for pre-release or "0.70.2.0" for release
    [String]$FileVersion = "",

    [switch]$RunTests,
    [switch]$Incremental,
    [switch]$UseVS,
    [switch]$ConfigureOnly
)

function Find-Path($exename) {
    $v = (get-command $exename -ErrorAction SilentlyContinue)
    if ($v.Path) {
        return $v.Path
    }
    else {
        throw "Could not find $exename"
    }
}

function Find-VS-Path() {
    $vsWhere = (get-command "vswhere.exe" -ErrorAction SilentlyContinue)
    if ($vsWhere) {
        $vsWhere = $vsWhere.Path
    }
    else {
        $vsWhere = "${Env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" 
    }

    if (Test-Path $vsWhere) {
        $versionJson = & $vsWhere -format json
        $versionJson = & $vsWhere -format json -version 16
        $versionJson = $versionJson | ConvertFrom-Json
    } else {
        $versionJson = @()
    }

    if ($versionJson.Length -gt 1) {
      Write-Warning 'More than one VS install detected, picking the first one';
      $versionJson = $versionJson[0];
    }

    if ($versionJson.installationPath) {
        $vcVarsPath = "$($versionJson.installationPath)\VC\Auxiliary\Build\vcvarsall.bat"
        if (Test-Path $vcVarsPath) {
            return $vcVarsPath
        } else {
            throw "Could not find vcvarsall.bat at expected Visual Studio installation path: $vcVarsPath"
        }
    } else {
        throw "Could not find Visual Studio installation path"
    }
}

function Get-VCVarsParam($plat = "x64", $arch = "win32") {
    $args_ = switch ($plat)
    {
        "x64" {"x64"}
        "x86" {"x64_x86"}
        "arm64" {"x64_arm64"}
        default { "x64" }
    }

    if ($arch -eq "uwp") {
        $args_ = "$args_ uwp"
    }

    if ($SDKVersion) {
        $args_ = "$args_ $SDKVersion"
    }

    # Spectre mitigations (for SDL)
    $args_ = "$args_ -vcvars_spectre_libs=spectre"

    return $args_
}

function Get-CMakeConfiguration($config) {
    $val = switch ($config)
    {
        "debug" {"FastDebug"}
        "release" {"Release"}
        default {"Debug"}
    }

    return $val
}

function Invoke-RemoveUnusedFilesForComponentGovernance() {
    Remove-Item -Path ".\unsupported\juno" -Force -Recurse
}

function Invoke-Environment($Command, $arg) {
    $Command = "`"" + $Command + "`" " + $arg
    Write-Host "Running command [ $Command ]"
    cmd /c "$Command && set" | . { process {
        if ($_ -match '^([^=]+)=(.*)') {
            [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2])
        }
    }}
}

function Invoke-UpdateReleaseVersion($SourcesPath, $ReleaseVersion, $FileVersion) {
    if ([String]::IsNullOrWhiteSpace($ReleaseVersion)) {
        return
    }

    $ProjectVersion = $ReleaseVersion
    if ($ReleaseVersion.StartsWith("0.0.0")) {
        # Use file version as a project version for pre-release builds
        # because CMake does not accept our pre-release version format.
        $ProjectVersion = $FileVersion
    }

    $filePath1 = Join-Path $SourcesPath "CMakeLists.txt"
    $versionRegex1 = '        VERSION .*'
    $versionStr1 = '        VERSION ' + $ProjectVersion
    $content1 = (Get-Content $filePath1) -replace $versionRegex1, $versionStr1 -join "`r`n"
    [IO.File]::WriteAllText($filePath1, $content1)

    $filePath2 = Join-Path (Join-Path $SourcesPath "npm") "package.json"
    $versionRegex2 = '"version": ".*",'
    $versionStr2 = '"version": "' + $ReleaseVersion + '",'
    $content2 = (Get-Content $filePath2) -replace $versionRegex2, $versionStr2 -join "`r`n"
    [IO.File]::WriteAllText($filePath2, $content2)

    Write-Host "Release version set to $ReleaseVersion"
    Write-Host "Project version set to $ProjectVersion"
}

function get-CommonArgs($Platform, $Configuration, $AppPlatform, [ref]$genArgs) {
    if ($UseVS.IsPresent) {
        # TODO: use VS version chosen before
        $genArgs.Value += '-G "Visual Studio 16 2019"'
        $cmakePlatform = $Platform;
        if ($cmakePlatform -eq 'x86') {
            $cmakePlatform = 'Win32';
        }
        $genArgs.Value += ('-A {0}' -f $cmakePlatform)
    } else {
        $genArgs.Value += '-G Ninja'
    }

    $genArgs.Value += ('-DCMAKE_BUILD_TYPE={0}' -f (Get-CMakeConfiguration $Configuration))

    $genArgs.Value += '-DHERMESVM_PLATFORM_LOGGING=On'

    if (![String]::IsNullOrWhiteSpace($FileVersion)) {
        $genArgs.Value += '-DHERMES_FILE_VERSION=' + $FileVersion
    }

    Write-Host "HERMES_FILE_VERSION is $FileVersion"
}

function Invoke-BuildImpl($SourcesPath, $buildPath, $genArgs, $targets, $incrementalBuild, $Platform, $Configuration, $AppPlatform) {

    Write-Host "Invoke-BuildImpl called with SourcesPath: " $SourcesPath", buildPath: " $buildPath  ", genArgs: " $genArgs ", targets: " $targets ", incrementalBuild: " $incrementalBuild", Platform: " $Platform ", Configuration: " $Configuration ", AppPlatform: " $AppPlatform

    # Retain the build folder for incremental builds only.
    if (!$incrementalBuild -and (Test-Path -Path $buildPath)) {
        Remove-Item $buildPath -Recurse -ErrorAction Ignore
    }
    
    New-Item -ItemType "directory" -Path $buildPath -ErrorAction Ignore | Out-Null
    Push-Location $buildPath
    try {
        $genCall = ('cmake {0}' -f ($genArgs -Join ' ')) + " $SourcesPath";
        Write-Host $genCall
        $ninjaCmd = "ninja"

        foreach ( $target in $targets )
        {
            $ninjaCmd = $ninjaCmd + " " + $target
        }

        Write-Host $ninjaCmd

        # See https://developercommunity.visualstudio.com/content/problem/257260/vcvarsallbat-reports-the-input-line-is-too-long-if.html
        $Bug257260 = $false

        if ($Bug257260) {
            Invoke-Environment $VCVARS_PATH (Get-VCVarsParam $Platform $AppPlatform)
            Invoke-Expression $genCall
            
            if ($ConfigureOnly.IsPresent){
                exit 0;
            }

            if ($UseVS.IsPresent) {
                exit  1;
            } else {
                ninja $ninjaCmd
            }

        } else {
            $GenCmd = "`"$VCVARS_PATH`" $(Get-VCVarsParam $Platform $AppPlatform) && $genCall 2>&1"
            Write-Host "Command: $GenCmd"
            cmd /c $GenCmd

            if ($ConfigureOnly.IsPresent){
                exit 0;
            }

            $NinjaCmd = "`"$VCVARS_PATH`" $(Get-VCVarsParam $Platform $AppPlatform) && ${ninjaCmd} 2>&1"
            Write-Host "Command: $NinjaCmd"
            cmd /c $NinjaCmd
        }
    } finally {
        Pop-Location
    }
}

function Invoke-Compiler-Build($SourcesPath, $buildPath, $Platform, $Configuration, $AppPlatform, $incrementalBuild) {
    $genArgs = @();
    get-CommonArgs $Platform $Configuration $AppPlatform ([ref]$genArgs)
    Invoke-BuildImpl $SourcesPath $buildPath $genArgs @('hermes','hermesc') $incrementalBuild $Platform $Configuration $AppPlatform
}

function Invoke-Dll-Build($SourcesPath, $buildPath, $compilerAndToolsBuildPath, $Platform, $Configuration, $AppPlatform, $incrementalBuild) {
    $genArgs = @();
    get-CommonArgs $Platform $Configuration $AppPlatform ([ref]$genArgs)

    $targets = @('libhermes');

    $genArgs += '-DHERMES_ENABLE_DEBUGGER=ON'

    $genArgs += '-DHERMES_MSVC_USE_PLATFORM_UNICODE_WINGLOB=ON'

    if ($AppPlatform -eq "uwp") {
        # Link against default ICU libraries in Windows 10.
        $genArgs += '-DHERMES_MSVC_USE_PLATFORM_UNICODE_WINGLOB=OFF'
    } else {
        # Use our custom WinGlob/NLS based implementation of unicode stubs, to avoid depending on the runtime ICU library.
        $genArgs += '-DHERMES_MSVC_USE_PLATFORM_UNICODE_WINGLOB=ON'
    }

    if ($AppPlatform -eq "uwp") {
        $genArgs += '-DCMAKE_SYSTEM_NAME=WindowsStore'
        $genArgs += '-DCMAKE_SYSTEM_VERSION="10.0.17763.0"'
        $genArgs += "-DIMPORT_HERMESC=$compilerAndToolsBuildPath\ImportHermesc.cmake"
    } elseif ($Platform -eq "arm64") {
        $genArgs += '-DHERMES_MSVC_ARM64=On'
        $genArgs += "-DIMPORT_HERMESC=$compilerAndToolsBuildPath\ImportHermesc.cmake"
    }

    Invoke-BuildImpl $SourcesPath $buildPath $genArgs $targets $incrementalBuild $Platform $Configuration $AppPlatform
}

function Invoke-Test-Build($SourcesPath, $buildPath, $compilerAndToolsBuildPath, $Platform, $Configuration, $AppPlatform,  $incrementalBuild) {
    $genArgs = @();
    get-CommonArgs([ref]$genArgs)

    $genArgs += '-DHERMES_ENABLE_DEBUGGER=On'

    if ($AppPlatform -eq "uwp") {
        $genArgs += '-DCMAKE_SYSTEM_NAME=WindowsStore'
        $genArgs += '-DCMAKE_SYSTEM_VERSION="10.0.17763"'
    } elseif ($Platform -eq "arm64") {
        $genArgs += '-DHERMES_MSVC_ARM64=On'
        $genArgs += "-DIMPORT_HERMESC=$compilerAndToolsBuildPath\ImportHermesc.cmake"
    }

    Invoke-BuildImpl $SourcesPath, $buildPath, $genArgs, @('check-hermes') $incrementalBuild $Platform $Configuration $AppPlatform
}

function Invoke-BuildAndCopy($SourcesPath, $WorkSpacePath, $OutputPath, $Platform, $Configuration, $AppPlatform) {

    Write-Host "Invoke-Build called with SourcesPath: " $SourcesPath", WorkSpacePath: " $WorkSpacePath  ", OutputPath: " $OutputPath ", Platform: " $Platform ", Configuration: " $Configuration ", AppPlatform: " $AppPlatform
    $Triplet = "$AppPlatform-$Platform-$Configuration"
    $compilerAndToolsBuildPath = Join-Path $WorkSpacePath "build\tools"
    $compilerPath = Join-Path $compilerAndToolsBuildPath "bin\hermesc.exe"
    
    # Build compiler if it doesn't exist (TODO::To be precise, we need it only when building for uwp i.e. cross compilation !). 
    if (!(Test-Path -Path $compilerPath)) {
        Invoke-Compiler-Build $SourcesPath $compilerAndToolsBuildPath $toolsPlatform $toolsConfiguration "win32" $True
    }
    
    $buildPath = Join-Path $WorkSpacePath "build\$Triplet"
    
    Invoke-Dll-Build $SourcesPath $buildPath $compilerAndToolsBuildPath $Platform $Configuration $AppPlatform $Incremental.IsPresent

    if ($RunTests.IsPresent) {
        Invoke-Test-Build $SourcesPath $buildPath $compilerAndToolsBuildPath $Platform $Configuration $AppPlatform $Incremental.IsPresent
    }
    
    $finalOutputPath = "$OutputPath\lib\native\$AppPlatform\$Configuration\$Platform";
    if (!(Test-Path -Path $finalOutputPath)) {
        New-Item -ItemType "directory" -Path $finalOutputPath | Out-Null
    }

    $RNDIR = Join-Path $buildPath "_deps\reactnative-src"

    Copy-Item "$buildPath\API\hermes\hermes.dll" -Destination $finalOutputPath -force | Out-Null
    Copy-Item "$buildPath\API\hermes\hermes.lib" -Destination $finalOutputPath -force | Out-Null
    Copy-Item "$buildPath\API\hermes\hermes.pdb" -Destination $finalOutputPath -force | Out-Null

    if (!(Test-Path -Path "$OutputPath\lib\uap\")) {
        New-Item -ItemType "directory" -Path "$OutputPath\lib\uap\" | Out-Null
        New-Item -Path "$OutputPath\lib\uap\" -Name "_._" -ItemType File
    }

    $toolsPath = "$OutputPath\tools\native\$toolsConfiguration\$toolsPlatform"
    if (!(Test-Path -Path $toolsPath)) {
        New-Item -ItemType "directory" -Path $toolsPath | Out-Null
    }
    Copy-Item "$compilerAndToolsBuildPath\bin\hermes.exe" -Destination $toolsPath

    $flagsPath = "$OutputPath\build\native\flags\$Triplet"
    if (!(Test-Path -Path $flagsPath)) {
        New-Item -ItemType "directory" -Path $flagsPath | Out-Null
    }
    Copy-Item "$buildPath\build.ninja" -Destination $flagsPath -force | Out-Null

    Copy-Headers $SourcesPath $WorkSpacePath $OutputPath $Platform $Configuration $AppPlatform $RNDIR
}

function Copy-Headers($SourcesPath, $WorkSpacePath, $OutputPath, $Platform, $Configuration, $AppPlatform, $RNDIR) {

    if (!(Test-Path -Path "$OutputPath\build\native\include\jsi")) {
        New-Item -ItemType "directory" -Path "$OutputPath\build\native\include\jsi" | Out-Null
    }

    if (!(Test-Path -Path "$OutputPath\build\native\include\node-api")) {
        New-Item -ItemType "directory" -Path "$OutputPath\build\native\include\node-api" | Out-Null
    }

    if (!(Test-Path -Path "$OutputPath\build\native\include\hermes")) {
        New-Item -ItemType "directory" -Path "$OutputPath\build\native\include\hermes" | Out-Null
    }

    Copy-Item "$SourcesPath\API\jsi\jsi\*" -Destination "$OutputPath\build\native\include\jsi" -force -Recurse

    Copy-Item "$SourcesPath\API\node-api\js_native_api.h" -Destination "$OutputPath\build\native\include\node-api" -force
    Copy-Item "$SourcesPath\API\node-api\js_native_api_types.h" -Destination "$OutputPath\build\native\include\node-api" -force
    Copy-Item "$SourcesPath\API\node-api\js_runtime_api.h" -Destination "$OutputPath\build\native\include\node-api" -force

    Copy-Item "$SourcesPath\API\hermes\hermes_api.h" -Destination "$OutputPath\build\native\include\hermes" -force
}

function Invoke-PrepareNugetPackage($SourcesPath, $WorkSpacePath, $OutputPath, $Platform, $Configuration, $AppPlatform) {
    $nugetPath = Join-Path $OutputPath "nuget"
    if (!(Test-Path -Path $nugetPath)) {
        New-Item -ItemType "directory" -Path $nugetPath | Out-Null
    }

    # copy misc files for NuGet packaging.
    if (!(Test-Path -Path "$OutputPath\license")) {
        New-Item -ItemType "directory" -Path "$OutputPath\license" | Out-Null
    }

    Copy-Item "$SourcesPath\LICENSE" -Destination "$OutputPath\license\" -force
    Copy-Item "$SourcesPath\.ado\NOTICE.txt" -Destination "$OutputPath\license\" -force
    Copy-Item "$SourcesPath\.ado\Microsoft.JavaScript.Hermes.targets" -Destination "$OutputPath\build\native\Microsoft.JavaScript.Hermes.targets" -force

    # process version information

    $gitRevision = ((git rev-parse --short HEAD) | Out-String).Trim()

    $npmPackage = (Get-Content (Join-Path $SourcesPath "npm\package.json") | Out-String | ConvertFrom-Json).version

    (Get-Content "$SourcesPath\.ado\Microsoft.JavaScript.Hermes.nuspec") -replace ('VERSION_DETAILS', "Hermes version: $npmPackage; Git revision: $gitRevision") | Set-Content "$OutputPath\Microsoft.JavaScript.Hermes.nuspec"
    (Get-Content "$SourcesPath\.ado\Microsoft.JavaScript.Hermes.Fat.nuspec") -replace ('VERSION_DETAILS', "Hermes version: $npmPackage; Git revision: $gitRevision") | Set-Content "$OutputPath\Microsoft.JavaScript.Hermes.Fat.nuspec"
    
    $npmPackage | Set-Content "$OutputPath\version"
}

Invoke-RemoveUnusedFilesForComponentGovernance
$StartTime = (Get-Date)

$VCVARS_PATH = Find-VS-Path

if (!(Test-Path -Path $WorkSpacePath)) {
    New-Item -ItemType "directory" -Path $WorkSpacePath | Out-Null
}

Push-Location $WorkSpacePath
try {
    Invoke-UpdateReleaseVersion -SourcesPath $SourcesPath -ReleaseVersion $ReleaseVersion -FileVersion $FileVersion

    # run the actual builds and copy artefacts
    foreach ($Plat in $Platform) {
        foreach ($Config in $Configuration) {
            Invoke-BuildAndCopy -SourcesPath $SourcesPath -WorkSpacePath $WorkSpacePath -OutputPath $OutputPath -Platform $Plat -Configuration $Config -AppPlatform $AppPlatform
            Invoke-PrepareNugetPackage -SourcesPath $SourcesPath -WorkSpacePath $WorkSpacePath -OutputPath $OutputPath -Platform $Plat -Configuration $Config -AppPlatform $AppPlatform
        }
    }
} finally {
    Pop-Location
}

$elapsedTime = $(get-date) - $StartTime
$totalTime = "{0:HH:mm:ss}" -f ([datetime]$elapsedTime.Ticks)
Write-Host "Build took $totalTime to run"