[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter()]
    [string]$RepositoryRoot = "",

    [Parameter()]
    [string[]]$HoudiniVersions = @("20.0", "20.5", "21.0", "22.0"),

    [Parameter()]
    [string]$ConverterExecutable = "",

    [Parameter()]
    [switch]$Uninstall
)

$ErrorActionPreference = "Stop"

function Convert-ToPackagePath {
    param([Parameter(Mandatory)][string]$Path)

    return ([System.IO.Path]::GetFullPath($Path) -replace "\\", "/")
}

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $sourceRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
    $installRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
    if (Test-Path (Join-Path $sourceRoot "python\houio_hom")) {
        $RepositoryRoot = $sourceRoot
    } elseif (Test-Path (Join-Path $installRoot "share\houio\python\houio_hom")) {
        $RepositoryRoot = $installRoot
    } else {
        throw "Could not infer a HouIO source or install root. Pass -RepositoryRoot explicitly."
    }
}
$resolvedRoot = Convert-ToPackagePath -Path $RepositoryRoot

$pythonRoot = Join-Path $RepositoryRoot "python"
if (-not (Test-Path (Join-Path $pythonRoot "houio_hom"))) {
    $pythonRoot = Join-Path $RepositoryRoot "share\houio\python"
}
if (-not (Test-Path (Join-Path $pythonRoot "houio_hom"))) {
    throw "Could not find the installed houio_hom package under $RepositoryRoot"
}
$resolvedPythonRoot = Convert-ToPackagePath -Path $pythonRoot

if ([string]::IsNullOrWhiteSpace($ConverterExecutable)) {
    $converterCandidates = @(
        (Join-Path $RepositoryRoot "build\windows-msvc-release\houio_convert.exe")
        (Join-Path $RepositoryRoot "build\windows-msvc-release\Release\houio_convert.exe")
        (Join-Path $RepositoryRoot "build\windows-gcc-mingw\houio_convert.exe")
        (Join-Path $RepositoryRoot "build\windows-gcc-mingw\Release\houio_convert.exe")
        (Join-Path $RepositoryRoot "bin\houio_convert.exe")
        (Join-Path $RepositoryRoot "bin\houio_convert")
    )
    foreach ($candidateConverter in $converterCandidates) {
        if (Test-Path $candidateConverter) {
            $ConverterExecutable = $candidateConverter
            break
        }
    }
}
if (-not [string]::IsNullOrWhiteSpace($ConverterExecutable) -and
    -not (Test-Path -LiteralPath $ConverterExecutable -PathType Leaf)) {
    throw "Could not find houio_convert at $ConverterExecutable"
}
$resolvedConverter = if ([string]::IsNullOrWhiteSpace($ConverterExecutable)) {
    ""
} else {
    Convert-ToPackagePath -Path $ConverterExecutable
}

$documentsDirectory = [Environment]::GetFolderPath([Environment+SpecialFolder]::MyDocuments)
if ([string]::IsNullOrWhiteSpace($documentsDirectory)) {
    $documentsDirectory = Join-Path $HOME "Documents"
}

foreach ($houdiniVersion in $HoudiniVersions) {
    $packageDirectory = Join-Path $documentsDirectory "houdini$houdiniVersion\packages"
    $packagePath = Join-Path $packageDirectory "houio.json"

    if ($Uninstall) {
        if (Test-Path $packagePath) {
            if ($PSCmdlet.ShouldProcess($packagePath, "Remove HouIO HOM package")) {
                Remove-Item -LiteralPath $packagePath -Force
                Write-Host "Removed $packagePath"
            }
        }
        continue
    }

    $packageEnvironment = @(
        [ordered]@{ HOUIO_ROOT = $resolvedRoot }
        [ordered]@{ HOUIO_PYTHON_ROOT = $resolvedPythonRoot }
        [ordered]@{ PYTHONPATH = '$HOUIO_PYTHON_ROOT;$PYTHONPATH' }
        [ordered]@{ HOUIO_BLOSC_LIBRARY = '$HFS/bin/blosc.dll' }
    )
    if (-not [string]::IsNullOrWhiteSpace($resolvedConverter)) {
        $packageEnvironment += [ordered]@{ HOUIO_CONVERT_EXECUTABLE = $resolvedConverter }
    }

    $package = [ordered]@{
        enable = $true
        env = $packageEnvironment
    }
    $packageJson = $package | ConvertTo-Json -Depth 5

    if ($PSCmdlet.ShouldProcess($packagePath, "Install HouIO HOM package")) {
        New-Item -ItemType Directory -Path $packageDirectory -Force | Out-Null
        $utf8WithoutBom = New-Object System.Text.UTF8Encoding($false)
        [System.IO.File]::WriteAllText($packagePath, $packageJson, $utf8WithoutBom)
        Write-Host "Installed $packagePath"
    }
}
