[cmdletbinding(SupportsShouldProcess=$True)]
param(
    [Parameter(Mandatory=$False)]
    [string]$RN_CLONE_URL = "https://github.com/facebook/react-native.git",

    [Parameter(Mandatory=$False)]
    [string]$RN_PATH = "react-native-clone",

    [Parameter(Mandatory=$False)]
    [string]$COMMIT_HASH="5127c71d6",

    [Parameter(Mandatory=$False)]
    [string]$FOLLY_EXTRACTED="folly_src"
)



if (!(Test-Path -Path $RN_PATH)) {
    New-Item -ItemType "directory" -Path $RN_PATH | Out-Null
}

pushd $RN_PATH
Invoke-Expression "git clone $RN_CLONE_URL"
pushd react-native
Invoke-Expression "git checkout $COMMIT_HASH"
popd
popd