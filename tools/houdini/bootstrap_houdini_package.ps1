[CmdletBinding()]
param(
    [Parameter()]
    [string]$SourceRoot = "",

    [Parameter()]
    [string]$BootstrapDirectory = "",

    [Parameter()]
    [string]$HoudiniVersion = "",

    [Parameter()]
    [ValidateSet("houdini", "hython")]
    [string]$Application = "houdini",

    [Parameter()]
    [string]$HoudiniExecutable = "",

    [Parameter()]
    [string[]]$ApplicationArguments = @(),

    [Parameter()]
    [switch]$ValidateOnly,

    [Parameter()]
    [switch]$KeepBootstrap
)

$ErrorActionPreference = "Stop"

function Resolve-AbsolutePath {
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string]$Description
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "$Description is empty."
    }

    try {
        return [System.IO.Path]::GetFullPath($Path)
    }
    catch {
        throw "Invalid $Description '$Path': $($_.Exception.Message)"
    }
}

function Convert-ToPackagePath {
    param([Parameter(Mandatory)][string]$Path)

    return ((Resolve-AbsolutePath -Path $Path -Description "package path") -replace "\\", "/")
}

function Find-HoudiniExecutable {
    param(
        [Parameter(Mandatory)]
        [string]$ApplicationName,

        [Parameter()]
        [string]$RequestedVersion = ""
    )

    $executableName = "$ApplicationName.exe"

    if (-not [string]::IsNullOrWhiteSpace($env:HFS)) {
        $fromHfs = Join-Path $env:HFS "bin\$executableName"
        if (Test-Path -LiteralPath $fromHfs -PathType Leaf) {
            if (
                [string]::IsNullOrWhiteSpace($RequestedVersion) -or
                ([System.IO.Path]::GetFileName($env:HFS) -like "Houdini $RequestedVersion*")
            ) {
                return (Resolve-AbsolutePath -Path $fromHfs -Description "Houdini executable")
            }
        }
    }

    $installRoot = Join-Path $env:ProgramFiles "Side Effects Software"
    if (-not (Test-Path -LiteralPath $installRoot -PathType Container)) {
        throw "Could not find the SideFX installation directory: $installRoot"
    }

    $candidates = @(
        Get-ChildItem -LiteralPath $installRoot -Directory |
            Where-Object {
                $_.Name -like "Houdini *" -and
                (
                    [string]::IsNullOrWhiteSpace($RequestedVersion) -or
                    $_.Name -like "Houdini $RequestedVersion*"
                )
            } |
            ForEach-Object {
                $versionText = $_.Name.Substring("Houdini ".Length)
                try {
                    $parsedVersion = [version]$versionText
                }
                catch {
                    $parsedVersion = $null
                }

                $candidateExecutable = Join-Path $_.FullName "bin\$executableName"
                if ($null -ne $parsedVersion -and $parsedVersion.Major -ge 20 -and (Test-Path -LiteralPath $candidateExecutable -PathType Leaf)) {
                    [pscustomobject]@{
                        Version = $parsedVersion
                        Path = $candidateExecutable
                    }
                }
            } |
            Sort-Object Version -Descending
    )

    if ($candidates.Count -eq 0) {
        $versionMessage = if ([string]::IsNullOrWhiteSpace($RequestedVersion)) {
            "20.0 or newer"
        }
        else {
            $RequestedVersion
        }
        throw "Could not find $ApplicationName for Houdini $versionMessage. Pass -HoudiniExecutable explicitly."
    }

    return (Resolve-AbsolutePath -Path $candidates[0].Path -Description "Houdini executable")
}

if ([string]::IsNullOrWhiteSpace($SourceRoot)) {
    $SourceRoot = $PSScriptRoot
}
$resolvedSourceRoot = Resolve-AbsolutePath -Path $SourceRoot -Description "package source root"

$packageFile = Join-Path $resolvedSourceRoot "houio.json"
$contentDirectory = Join-Path $resolvedSourceRoot "houio"
$converterExecutable = Join-Path $contentDirectory "bin\houio_convert.exe"

if (-not (Test-Path -LiteralPath $packageFile -PathType Leaf)) {
    throw "Missing package file: $packageFile"
}
if (-not (Test-Path -LiteralPath $contentDirectory -PathType Container)) {
    throw "Missing package content directory: $contentDirectory"
}
if (-not (Test-Path -LiteralPath $converterExecutable -PathType Leaf)) {
    throw "Missing converter executable: $converterExecutable"
}

if ([string]::IsNullOrWhiteSpace($BootstrapDirectory)) {
    $BootstrapDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("houio-bootstrap-" + [guid]::NewGuid().ToString("N"))
}
$resolvedBootstrapDirectory = Resolve-AbsolutePath -Path $BootstrapDirectory -Description "bootstrap directory"
if (Test-Path -LiteralPath $resolvedBootstrapDirectory) {
    throw "Bootstrap directory must not already exist: $resolvedBootstrapDirectory"
}
New-Item -ItemType Directory -Path $resolvedBootstrapDirectory | Out-Null
$bootstrapDirectoryCreatedByScript = $true
$packageDirectory = Join-Path $resolvedBootstrapDirectory "packages"
$userPreferencesDirectory = Join-Path $resolvedBootstrapDirectory "user\houdini__HVER__"
$loaderPath = Join-Path $packageDirectory "houio_loader.json"

New-Item -ItemType Directory -Path $packageDirectory -Force | Out-Null
New-Item -ItemType Directory -Path (Split-Path -Parent $userPreferencesDirectory) -Force | Out-Null

$loader = [ordered]@{
    enable = $true
    load_package_once = $true
    show = $true
    package_path = Convert-ToPackagePath -Path $resolvedSourceRoot
}
$loaderJson = $loader | ConvertTo-Json -Depth 4
$utf8WithoutBom = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText($loaderPath, $loaderJson, $utf8WithoutBom)

Write-Host "HouIO bootstrap package: $resolvedSourceRoot"
Write-Host "Isolated package directory: $packageDirectory"
Write-Host "Isolated user preferences: $userPreferencesDirectory"
Write-Host "No package files were installed."

if ($ValidateOnly) {
    if (-not $KeepBootstrap -and $bootstrapDirectoryCreatedByScript) {
        Remove-Item -LiteralPath $resolvedBootstrapDirectory -Recurse -Force
    }
    return
}

$environmentNames = @(
    "HOUDINI_PACKAGE_DIR",
    "HOUDINI_USER_PREF_DIR",
    "HOUDINI_NO_ENV_FILE"
)
$savedEnvironment = @{}
foreach ($name in $environmentNames) {
    $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, "Process")
}

try {
    if ([string]::IsNullOrWhiteSpace($HoudiniExecutable)) {
        $resolvedHoudiniExecutable = Find-HoudiniExecutable -ApplicationName $Application -RequestedVersion $HoudiniVersion
    }
    else {
        $resolvedHoudiniExecutable = Resolve-AbsolutePath -Path $HoudiniExecutable -Description "Houdini executable"
        if (-not (Test-Path -LiteralPath $resolvedHoudiniExecutable -PathType Leaf)) {
            throw "Houdini executable does not exist: $resolvedHoudiniExecutable"
        }
    }

    $env:HOUDINI_PACKAGE_DIR = $packageDirectory
    $env:HOUDINI_USER_PREF_DIR = $userPreferencesDirectory
    $env:HOUDINI_NO_ENV_FILE = "1"

    Write-Host "Launching: $resolvedHoudiniExecutable"
    $processArguments = @{
        FilePath = $resolvedHoudiniExecutable
        Wait = $true
        PassThru = $true
    }
    if ($ApplicationArguments.Count -gt 0) {
        $processArguments.ArgumentList = $ApplicationArguments
    }
    $process = Start-Process @processArguments
    if ($process.ExitCode -ne 0) {
        throw "$Application exited with code $($process.ExitCode)."
    }
    Write-Host "$Application bootstrap session completed successfully."
}
finally {
    foreach ($name in $environmentNames) {
        $savedValue = $savedEnvironment[$name]
        if ($null -eq $savedValue) {
            Remove-Item "Env:$name" -ErrorAction SilentlyContinue
        }
        else {
            [Environment]::SetEnvironmentVariable($name, $savedValue, "Process")
        }
    }

    if (
        (-not $KeepBootstrap) -and
        $bootstrapDirectoryCreatedByScript -and
        (Test-Path -LiteralPath $resolvedBootstrapDirectory)
    ) {
        Remove-Item -LiteralPath $resolvedBootstrapDirectory -Recurse -Force
    }
}
