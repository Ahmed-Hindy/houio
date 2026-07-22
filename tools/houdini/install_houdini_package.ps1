[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter()]
    [string]$SourceRoot = $PSScriptRoot,

    [Parameter()]
    [string[]]$HoudiniVersions = @("21.0", "22.0"),

    [Parameter()]
    [string]$InstallDirectory = "",

    [Parameter()]
    [switch]$Uninstall,

    [Parameter()]
    [switch]$KeepFiles
)

$ErrorActionPreference = "Stop"

function Convert-ToPackagePath {
    param([Parameter(Mandatory)][string]$Path)

    return ([System.IO.Path]::GetFullPath($Path) -replace "\\", "/")
}

function Get-PackageDirectories {
    param([Parameter(Mandatory)][string]$HoudiniVersion)

    $directories = [System.Collections.Generic.List[string]]::new()
    $documentsDirectory = [Environment]::GetFolderPath(
        [Environment+SpecialFolder]::MyDocuments
    )
    if (-not [string]::IsNullOrWhiteSpace($documentsDirectory)) {
        $directories.Add((Join-Path $documentsDirectory "houdini$HoudiniVersion\packages"))
    }

    if (-not [string]::IsNullOrWhiteSpace($HOME)) {
        $homePackageDirectory = Join-Path $HOME "houdini$HoudiniVersion\packages"
        if (-not $directories.Contains($homePackageDirectory)) {
            $directories.Add($homePackageDirectory)
        }
    }
    return $directories
}

if ([string]::IsNullOrWhiteSpace($InstallDirectory)) {
    if ([string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
        throw "LOCALAPPDATA is unavailable. Pass -InstallDirectory explicitly."
    }
    $InstallDirectory = Join-Path $env:LOCALAPPDATA "HouIO"
}

$resolvedInstallDirectory = [System.IO.Path]::GetFullPath($InstallDirectory)
$loaderFileName = "houio_loader.json"

if ($Uninstall) {
    foreach ($houdiniVersion in $HoudiniVersions) {
        foreach ($packageDirectory in Get-PackageDirectories -HoudiniVersion $houdiniVersion) {
            $loaderPath = Join-Path $packageDirectory $loaderFileName
            if (Test-Path -LiteralPath $loaderPath) {
                if ($PSCmdlet.ShouldProcess($loaderPath, "Remove HouIO package loader")) {
                    Remove-Item -LiteralPath $loaderPath -Force
                    Write-Host "Removed $loaderPath"
                }
            }
        }
    }

    if (-not $KeepFiles -and (Test-Path -LiteralPath $resolvedInstallDirectory)) {
        if ($PSCmdlet.ShouldProcess($resolvedInstallDirectory, "Remove HouIO package files")) {
            Remove-Item -LiteralPath $resolvedInstallDirectory -Recurse -Force
            Write-Host "Removed $resolvedInstallDirectory"
        }
    }
    return
}

$resolvedSourceRoot = [System.IO.Path]::GetFullPath($SourceRoot)
$sourcePackageFile = Join-Path $resolvedSourceRoot "houio.json"
$sourceContentDirectory = Join-Path $resolvedSourceRoot "houio"
$sourceInstaller = Join-Path $resolvedSourceRoot "install_houdini_package.ps1"
if (-not (Test-Path -LiteralPath $sourcePackageFile -PathType Leaf)) {
    throw "Missing package file: $sourcePackageFile"
}
if (-not (Test-Path -LiteralPath $sourceContentDirectory -PathType Container)) {
    throw "Missing package content directory: $sourceContentDirectory"
}

if ($resolvedSourceRoot -ne $resolvedInstallDirectory) {
    if ($PSCmdlet.ShouldProcess($resolvedInstallDirectory, "Install HouIO package files")) {
        if (Test-Path -LiteralPath $resolvedInstallDirectory) {
            Remove-Item -LiteralPath $resolvedInstallDirectory -Recurse -Force
        }
        New-Item -ItemType Directory -Path $resolvedInstallDirectory -Force | Out-Null
        Copy-Item -LiteralPath $sourcePackageFile -Destination $resolvedInstallDirectory -Force
        Copy-Item -LiteralPath $sourceContentDirectory -Destination $resolvedInstallDirectory -Recurse -Force
        if (Test-Path -LiteralPath $sourceInstaller -PathType Leaf) {
            Copy-Item -LiteralPath $sourceInstaller -Destination $resolvedInstallDirectory -Force
        }
    }
}

$packageRoot = Convert-ToPackagePath -Path $resolvedInstallDirectory
$loader = [ordered]@{
    enable = $true
    load_package_once = $true
    show = $true
    package_path = $packageRoot
}
$loaderJson = $loader | ConvertTo-Json -Depth 4
$utf8WithoutBom = [System.Text.UTF8Encoding]::new($false)

foreach ($houdiniVersion in $HoudiniVersions) {
    foreach ($packageDirectory in Get-PackageDirectories -HoudiniVersion $houdiniVersion) {
        $loaderPath = Join-Path $packageDirectory $loaderFileName
        if ($PSCmdlet.ShouldProcess($loaderPath, "Install HouIO package loader")) {
            New-Item -ItemType Directory -Path $packageDirectory -Force | Out-Null
            [System.IO.File]::WriteAllText($loaderPath, $loaderJson, $utf8WithoutBom)
            Write-Host "Installed $loaderPath"
        }
    }
}

Write-Host "HouIO package files: $resolvedInstallDirectory"
Write-Host "Restart Houdini 21/22, or reload the package from Houdini 22's Package Browser."
