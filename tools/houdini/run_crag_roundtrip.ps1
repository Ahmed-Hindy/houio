[CmdletBinding()]
param(
    [string]$GeneratorVersion = "22.0.368",
    [string[]]$ValidationVersions = @("21.0.631", "22.0.368"),
    [string]$BuildPreset = "windows-msvc-release"
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
    $cmakeCommand = Get-Command cmake -ErrorAction Stop
    $cmake = $cmakeCommand.Source
}

Invoke-NativeCommand -Executable $cmake -Arguments @("--preset", $BuildPreset)
Invoke-NativeCommand -Executable $cmake -Arguments @("--build", "--preset", $BuildPreset, "--parallel")

$generatorHython = Get-HythonPath -Version $GeneratorVersion
$outputDirectory = Join-Path $repositoryRoot "build\crag\h$($GeneratorVersion.Split('.')[0])"
$sourceGeometry = Join-Path $outputDirectory "crag_tpose.bgeo"
$roundtripGeometry = Join-Path $outputDirectory "crag_tpose_houio.bgeo"
$roundtripExecutable = Join-Path $repositoryRoot "build\$BuildPreset\tests\houio_roundtrip_geometry.exe"

if (-not (Test-Path -LiteralPath $roundtripExecutable -PathType Leaf)) {
    throw "Round-trip executable was not found at: $roundtripExecutable"
}

Invoke-NativeCommand -Executable $generatorHython -Arguments @(
    (Join-Path $repositoryRoot "tools\houdini\generate_crag.py"),
    $sourceGeometry
)
Invoke-NativeCommand -Executable $roundtripExecutable -Arguments @($sourceGeometry, $roundtripGeometry)

foreach ($validationVersion in $ValidationVersions) {
    Write-Host "Validating with Houdini $validationVersion"
    $validationHython = Get-HythonPath -Version $validationVersion
    Invoke-NativeCommand -Executable $validationHython -Arguments @(
        (Join-Path $repositoryRoot "tools\houdini\validate_geometry.py"),
        $roundtripGeometry,
        "--expect-points",
        "90085",
        "--expect-primitives",
        "89942",
        "--expect-point-attribute",
        "P",
        "--expect-vertex-attribute",
        "N",
        "--expect-vertex-attribute",
        "uv",
        "--expect-primitive-attribute",
        "name",
        "--expect-primitive-attribute",
        "piece"
    )
    Invoke-NativeCommand -Executable $validationHython -Arguments @(
        (Join-Path $repositoryRoot "tools\houdini\compare_geometry.py"),
        $sourceGeometry,
        $roundtripGeometry,
        "--point-attribute",
        "P",
        "--vertex-attribute",
        "N",
        "--vertex-attribute",
        "uv",
        "--primitive-string-attribute",
        "name",
        "--primitive-int-attribute",
        "piece"
    )
}

Write-Host "Crag round-trip succeeded: $roundtripGeometry"
