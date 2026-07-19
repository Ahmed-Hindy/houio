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

$cmakeDirectory = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
$cmake = Join-Path $cmakeDirectory "cmake.exe"
$ctest = Join-Path $cmakeDirectory "ctest.exe"
if (-not (Test-Path -LiteralPath $cmake -PathType Leaf)) {
    $cmake = (Get-Command cmake -ErrorAction Stop).Source
}
if (-not (Test-Path -LiteralPath $ctest -PathType Leaf)) {
    $ctest = (Get-Command ctest -ErrorAction Stop).Source
}

$generatorHython = Get-HythonPath -Version $GeneratorVersion
Invoke-NativeCommand -Executable $cmake -Arguments @(
    "--preset",
    $BuildPreset,
    "-DHOUIO_HYTHON_EXECUTABLE=$generatorHython"
)
Invoke-NativeCommand -Executable $cmake -Arguments @(
    "--build",
    "--preset",
    $BuildPreset,
    "--parallel"
)
Invoke-NativeCommand -Executable $ctest -Arguments @(
    "--test-dir",
    (Join-Path $repositoryRoot "build\$BuildPreset"),
    "--output-on-failure",
    "-R",
    "^houio\.fixtures\."
)

$fixtureRoot = Join-Path $repositoryRoot "build\$BuildPreset\tests\fixtures"
$manifest = Join-Path $fixtureRoot "source\manifest.json"
$sourceDirectory = Join-Path $fixtureRoot "source"
$outputDirectory = Join-Path $fixtureRoot "output"
$validator = Join-Path $repositoryRoot "tools\houdini\validate_fixture_suite.py"

foreach ($validationVersion in $ValidationVersions) {
    Write-Host "Validating fixture suite with Houdini $validationVersion"
    $validationHython = Get-HythonPath -Version $validationVersion
    Invoke-NativeCommand -Executable $validationHython -Arguments @(
        $validator,
        $manifest,
        $sourceDirectory,
        $outputDirectory
    )
}

Write-Host "Fixture round-trips succeeded: $outputDirectory"
