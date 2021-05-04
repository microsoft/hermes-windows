[cmdletbinding(SupportsShouldProcess=$True)]
param(
    # What .tar.gz file should be extracted? Must exist.
    [Parameter(Mandatory=$True)]
    [string]$FileToExtract,

    # What folder should the files be extracted into? Does not need to exist
    [Parameter(Mandatory=$True)]
    [string]$TargetFolder,

    # Optionally increase the buffer size when unzipping the .gz part of the file
    # Larger sizes will increase speed for large files, at the expense of RAM usage
    [int]$BufferSize = 1024
)

function Calculate-TarFileName {
    param(
        [Parameter(Mandatory=$true)]
        [string] $targzFile
    )

    $targzFile.Substring(0, $targzFile.LastIndexOfAny('.'))
}

# https://www.codeproject.com/Tips/638039/GZipStream-length-when-uncompressed
function Original-GzipFileSize {
    param(
        [Parameter(Mandatory=$true)]
        [string] $gzipFile
    )
    
    $fs = New-Object System.IO.FileStream $gzipFile, ([IO.FileMode]::Open), ([IO.FileAccess]::Read), ([IO.FileShare]::Read)

    try
    {
        $fh = New-Object byte[](3)
        $fs.Read($fh, 0, 3) | Out-Null
        # If magic numbers are 31 and 139 and the deflation id is 8 then this is a file to process
        if ($fh[0] -eq 31 -and $fh[1] -eq 139 -and $fh[2] -eq 8) 
        {
            $ba = New-Object byte[](4)
            $fs.Seek(-4, [System.IO.SeekOrigin]::End) | Out-Null
            $fs.Read($ba, 0, 4) | Out-Null
                
            return [int32][System.BitConverter]::ToInt32($ba, 0)
        }
        else
        {
            throw "File '$gzipFile' does not have the correct gzip header"
        }
    }
    finally
    {
        $fs.Close()
    }
}

# https://stackoverflow.com/a/42165686/847953
function Expand-GZip {
    [cmdletbinding(SupportsShouldProcess=$True)]
    param(
        [Parameter(Mandatory=$true)]
        [string]$infile,
        [Parameter(Mandatory=$true)]
        [string]$outFile,
        [int]$bufferSize = 1024
    )
    $fileSize = Original-GzipFileSize $inFile
    $processed = 0

    if ($PSCmdlet.ShouldProcess($infile,"Expand gzip stream")) {
        $input = New-Object System.IO.FileStream $inFile, ([IO.FileMode]::Open), ([IO.FileAccess]::Read), ([IO.FileShare]::Read)
        $output = New-Object System.IO.FileStream $outFile, ([IO.FileMode]::Create), ([IO.FileAccess]::Write), ([IO.FileShare]::None)
        $gzipStream = New-Object System.IO.Compression.GzipStream $input, ([IO.Compression.CompressionMode]::Decompress)

        $buffer = New-Object byte[]($bufferSize)
        while($true){
        
            $pc = (($processed / $fileSize) * 100) % 100
            Write-Progress "Extracting tar from gzip" -PercentComplete $pc
        
            $read = $gzipstream.Read($buffer, 0, $bufferSize)

            $processed = $processed + $read

            if ($read -le 0)
            {
                Write-Progress "Extracting tar from gzip" -Completed
                break
            }
            $output.Write($buffer, 0, $read)
        }

        $gzipStream.Close()
        $output.Close()
        $input.Close()
    }
}

# https://stackoverflow.com/a/46876070/847953
function Extract-Tar {
    [cmdletbinding(SupportsShouldProcess=$True)]
    param(
        [Parameter(Mandatory=$true)]
        [string] $tarFile,
        [Parameter(Mandatory=$true)]
        [string] $dest
    )

    if ($PSCmdlet.ShouldProcess($tarFile,"Expand tar file")) {
        Expand-7Zip $tarFile $dest
    }
}

# https://stackoverflow.com/a/46876070/847953
function Ensure-7Zip {
    param(
        [string]$pathToModule = ".\7Zip4Powershell\1.9.0\7Zip4PowerShell.psd1" # Call "Save-Module -Name 7Zip4Powershell -Path ." to get the files.
    )

    if (-not (Get-Command Expand-7Zip -ErrorAction Ignore)) {
        if(Test-Path $pathToModule)
        {
            if ($PSCmdlet.ShouldProcess($pathToModule,"Install 7Zip module from local path")) {
                Write-Progress -Activity "Installing the 7Zip4PowerShell module" "Using local module" -PercentComplete 50
                Import-Module $pathToModule
                Write-Progress -Activity "Installing the 7Zip4PowerShell module" "Using local module" -Completed
            }
        }
        else
        {
            if ($PSCmdlet.ShouldProcess("PowerShell feed",'Install 7Zip module')) {
                Write-Progress  -Activity "Installing the 7Zip4PowerShell module" "Using public feed" -PercentComplete 50
                $progressPreference = 'silentlyContinue'
                Install-Package -Scope CurrentUser -Force 7Zip4PowerShell > $null
                $progressPreference = 'Continue'
                Write-Progress  -Activity "Installing the 7Zip4PowerShell module" "Using public feed" -Completed
            }
        }
    }
}

if(!(Test-Path $FileToExtract))
{
    throw "Source file '$FileToExtract' does not exist"
}

if(!$FileToExtract.EndsWith(".tar.gz", "CurrentCultureIgnoreCase"))
{
    throw "Source file '$FileToExtract' does not have a .tar.gz extension"
}

$FileToExtract = Resolve-Path $FileToExtract
$tarFile = Calculate-TarFileName $FileToExtract

Expand-GZip $FileToExtract $tarFile $BufferSize
Ensure-7Zip
Extract-Tar $tarFile $TargetFolder

if ($PSCmdlet.ShouldProcess($tarFile,'Remove temporary tar file')) {
    Remove-Item $tarFile
}