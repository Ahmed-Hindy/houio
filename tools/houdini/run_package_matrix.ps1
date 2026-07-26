[CmdletBinding()]
param(
    [string[]]$ValidationVersions = @("20.0.653", "20.5.410", "21.0.631", "22.0.368"),
    [string]$BuildPreset = "windows-msvc-release",
    [string]$PackageArchive = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Invoke-NativeCommand {
    param(
        [Parameter(Mandatory)]
        [string]$Executable,

        [Parameter(Mandatory)]
        [string[]]$Arguments
    )

    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $Executable $($Arguments -join ' ')"
    }
}

function Get-HythonPath {
    param(
        [Parameter(Mandatory)]
        [string]$Version
    )

    $path = "C:\Program Files\Side Effects Software\Houdini $Version\bin\hython.exe"
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Houdini $Version hython was not found at: $path"
    }
    return $path
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$vsDevShell = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1"
if (-not (Test-Path -LiteralPath $vsDevShell -PathType Leaf)) {
    throw "Visual Studio Developer PowerShell was not found at: $vsDevShell"
}

& $vsDevShell -Arch amd64 -HostArch amd64
Set-Location $repositoryRoot

$cmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path -LiteralPath $cmake -PathType Leaf)) {
    $cmake = (Get-Command cmake -ErrorAction Stop).Source
}

if ([string]::IsNullOrWhiteSpace($PackageArchive)) {
    Invoke-NativeCommand -Executable $cmake -Arguments @("--preset", $BuildPreset)
    Invoke-NativeCommand -Executable $cmake -Arguments @(
        "--build",
        "--preset",
        $BuildPreset,
        "--target",
        "houio_houdini_package",
        "--parallel"
    )

    $archives = @(Get-ChildItem -LiteralPath (Join-Path $repositoryRoot "build\$BuildPreset") `
        -Filter "houio-houdini-package-*-windows-x86_64.zip" -File |
        Sort-Object LastWriteTimeUtc -Descending)
    if ($archives.Count -eq 0) {
        throw "No Houdini package archive was generated under build\$BuildPreset"
    }
    $PackageArchive = $archives[0].FullName
}
else {
    $PackageArchive = (Resolve-Path -LiteralPath $PackageArchive).Path
}

$testScript = Join-Path $repositoryRoot "tools\houdini\test_houdini_package.py"
$cmakeTestScript = Join-Path $repositoryRoot "tests\run_houdini_package_test.cmake"

foreach ($validationVersion in $ValidationVersions) {
    Write-Host "Testing package with Houdini $validationVersion"
    $hython = Get-HythonPath -Version $validationVersion
    $extractDirectory = Join-Path $repositoryRoot "build\package-matrix\$validationVersion"
    Invoke-NativeCommand -Executable $cmake -Arguments @(
        "-DHOUIO_HOUDINI_PACKAGE_ARCHIVE=$PackageArchive",
        "-DHOUIO_HOUDINI_PACKAGE_EXTRACT_DIR=$extractDirectory",
        "-DHOUIO_HYTHON_EXECUTABLE=$hython",
        "-DHOUIO_HOUDINI_PACKAGE_TEST_SCRIPT=$testScript",
        "-P",
        $cmakeTestScript
    )
}

Write-Host "Houdini package matrix succeeded: $PackageArchive"
