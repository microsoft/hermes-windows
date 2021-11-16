[cmdletbinding(SupportsShouldProcess=$True)]
param(
    [Parameter(Mandatory=$False)]
    [string]$RN_CLONE_URL = "https://github.com/facebook/react-native.git",

    [Parameter(Mandatory=$False)]
    [string]$COMMIT_HASH="v0.66.0-rc.1",

    [Parameter(Mandatory=$False)]
    [switch]$Force
)

$RN_REPO_NAME = "react-native"
$RN_DIR="$PWD\$RN_REPO_NAME"

if($Force.IsPresent) {
    if (Test-Path -Path $RN_REPO_NAME){
        Remove-Item -Recurse -Force $RN_REPO_NAME  | Out-Null
    }
}

Invoke-Expression 'git config --local user.name "HermesDev"'
Invoke-Expression 'git config --local user.email "hermesdev@microsoft.com"'
Invoke-Expression 'git config --local core.autocrlf false'
Invoke-Expression 'git config --local core.filemode false'

# We have an existing checkout if git head exists.
$RN_REPO_HEAD="$RN_DIR\.git\HEAD"
if (!(Test-Path -Path $RN_REPO_HEAD)) {
    if (Test-Path -Path $RN_REPO_NAME) {
        Remove-Item -Recurse -Force $RN_REPO_NAME  | Out-Null
    }

    Invoke-Expression "git clone $RN_CLONE_URL"  | Out-Null
} 

Push-Location $RN_REPO_NAME

# A hack to avoid the bat files from autocrlfed which fails the subsequent checkout.
$RN_GIT_ATTRIBUTES="$RN_DIR\.gitattributes"
if (Test-Path -Path $RN_GIT_ATTRIBUTES) {
    Remove-Item -Force $RN_GIT_ATTRIBUTES | Out-Null
    Invoke-Expression "git add -A"  | Out-Null
    Invoke-Expression 'git commit -m "Temporary commit to create create clone"'  | Out-Null
    Invoke-Expression 'git reset --hard'  | Out-Null
}

Invoke-Expression "git checkout $COMMIT_HASH"  | Out-Null
Pop-Location

# extern IInspector &getInspectorInstance();
# extern __declspec(dllexport) IInspector &getInspectorInstance();
$RN_INSPECTORINTERFACES_H="$RN_DIR/ReactCommon/jsinspector/InspectorInterfaces.h"
(Get-Content -path $RN_INSPECTORINTERFACES_H ) -Replace "extern IInspector &getInspectorInstance", "extern __declspec(dllexport) IInspector& __cdecl getInspectorInstance" | Set-Content -Path $RN_INSPECTORINTERFACES_H | Out-Null

# extern void enableDebugging(
# extern __declspec(dllexport) void enableDebugging(
$RN_INSPECTOR_REGISTRATION_H="$RN_DIR/ReactCommon/hermes/inspector/chrome/Registration.h"
(Get-Content -path $RN_INSPECTOR_REGISTRATION_H) -Replace "extern void enableDebugging", "extern __declspec(dllexport) void __cdecl enableDebugging" | Set-Content -Path $RN_INSPECTOR_REGISTRATION_H | Out-Null
(Get-Content -path $RN_INSPECTOR_REGISTRATION_H) -Replace "extern void disableDebugging", "extern __declspec(dllexport) void __cdecl disableDebugging" | Set-Content -Path $RN_INSPECTOR_REGISTRATION_H | Out-Null

return $RN_DIR