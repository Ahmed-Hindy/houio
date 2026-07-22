[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter()]
    [string]$SourceRoot = "",

    [Parameter()]
    [string[]]$HoudiniVersions = @("20.0", "20.5", "21.0", "22.0"),

    [Parameter()]
    [string]$InstallDirectory = "",

    [Parameter()]
    [switch]$Install,

    [Parameter()]
    [switch]$Uninstall,

    [Parameter()]
    [switch]$KeepFiles
)

$ErrorActionPreference = "Stop"

if ($Install -and $Uninstall) {
    throw "Choose either -Install or -Uninstall, not both."
}
if (-not $Install -and -not $Uninstall) {
    throw "No action selected. Use bootstrap_houdini_package.ps1 for transient testing, or pass -Install for a persistent installation."
}
if ([string]::IsNullOrWhiteSpace($SourceRoot)) {
    $SourceRoot = $PSScriptRoot
}

function Resolve-AbsolutePath {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Description
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

function Get-PackageDirectories {
    param([Parameter(Mandatory)][string]$HoudiniVersion)

    $directories = [System.Collections.Generic.List[string]]::new()
    $documentsDirectory = [Environment]::GetFolderPath(
        [Environment+SpecialFolder]::MyDocuments
    )
    if (-not [string]::IsNullOrWhiteSpace($documentsDirectory)) {
        $directories.Add((Join-Path $documentsDirectory "houdini$HoudiniVersion\packages"))
        return $directories
    }

    if (-not [string]::IsNullOrWhiteSpace($HOME)) {
        $directories.Add((Join-Path $HOME "houdini$HoudiniVersion\packages"))
    }
    return $directories
}

if ([string]::IsNullOrWhiteSpace($InstallDirectory)) {
    if ([string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
        throw "LOCALAPPDATA is unavailable. Pass -InstallDirectory explicitly."
    }
    $InstallDirectory = Join-Path $env:LOCALAPPDATA "HouIO"
}

$resolvedInstallDirectory = Resolve-AbsolutePath -Path $InstallDirectory -Description "install directory"
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

$resolvedSourceRoot = Resolve-AbsolutePath -Path $SourceRoot -Description "package source root"
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

if ($WhatIfPreference) {
    Write-Host "Dry run complete. No files were installed."
    return
}

Write-Host "HouIO package files: $resolvedInstallDirectory"
Write-Host "Restart Houdini 20.0 or newer, or reload the package from Houdini 22's Package Browser."
