[CmdletBinding()]
param(
    [string[]]$ValidationVersions = @("20.0.653", "20.5.410", "21.0.631", "22.0.368")
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

function Invoke-CapturedNativeCommand {
    param(
        [Parameter(Mandatory)]
        [string]$Executable,

        [Parameter(Mandatory)]
        [string[]]$Arguments
    )

    $output = @(& $Executable @Arguments 2>&1)
    $exitCode = $LASTEXITCODE
    foreach ($line in $output) {
        Write-Host $line
    }
    if ($exitCode -ne 0) {
        throw "Command failed with exit code ${exitCode}: $Executable $($Arguments -join ' ')"
    }
    return ($output -join "`n")
}

function Get-HoudiniRoot {
    param(
        [Parameter(Mandatory)]
        [string]$Version
    )

    $root = "C:\Program Files\Side Effects Software\Houdini $Version"
    if (-not (Test-Path -LiteralPath $root -PathType Container)) {
        throw "Houdini $Version was not found at: $root"
    }
    return $root
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

$testScript = Join-Path $repositoryRoot "tools\houdini\test_native_rop.py"
$sceneFormatTestScript = Join-Path $repositoryRoot "tools\houdini\test_native_scene_formats.py"
foreach ($validationVersion in $ValidationVersions) {
    Write-Host "Building native HouIO ROP for Houdini $validationVersion"
    $houdiniRoot = Get-HoudiniRoot -Version $validationVersion
    $buildDirectory = Join-Path $repositoryRoot "build\native-rop\$validationVersion"

    Invoke-NativeCommand -Executable $cmake -Arguments @(
        "-S",
        $repositoryRoot,
        "-B",
        $buildDirectory,
        "-G",
        "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DHOUIO_BUILD_TESTS=OFF",
        "-DHOUIO_BUILD_EXAMPLES=OFF",
        "-DHOUIO_BUILD_TOOLS=OFF",
        "-DHOUIO_BUILD_HOUDINI_PLUGIN=ON",
        "-DHOUIO_WARNINGS_AS_ERRORS=ON",
        "-DHOUIO_HOUDINI_ROOT=$houdiniRoot"
    )
    Invoke-NativeCommand -Executable $cmake -Arguments @(
        "--build",
        $buildDirectory,
        "--target",
        "houio_houdini_rop",
        "--parallel"
    )

    $dsoDirectory = Join-Path $buildDirectory "houdini\hdk\dso"
    $pluginPath = Join-Path $dsoDirectory "ROP_HouIO.dll"
    if (-not (Test-Path -LiteralPath $pluginPath -PathType Leaf)) {
        throw "Native HouIO ROP was not generated: $pluginPath"
    }

    $hython = Join-Path $houdiniRoot "bin\hython.exe"
    $blosc = Join-Path $houdiniRoot "bin\blosc.dll"
    $outputDirectory = Join-Path $repositoryRoot "build\native-rop-test\$validationVersion"
    $env:HOUDINI_DSO_PATH = "$dsoDirectory;&"
    $env:HOUIO_BLOSC_LIBRARY = $blosc

    Write-Host "Testing native HouIO ROP with Houdini $validationVersion"
    $testOutput = Invoke-CapturedNativeCommand -Executable $hython -Arguments @(
        $testScript,
        $outputDirectory
    )
    if ($testOutput -match "OPUI_DialogPRM2|stepped on") {
        throw "Native ROP parameter dialog emitted duplicate-symbol warnings in Houdini $validationVersion"
    }

    $sceneOutputDirectory = Join-Path $repositoryRoot "build\native-scene-test\$validationVersion"
    Write-Host "Testing native Alembic/USD exports with Houdini $validationVersion"
    $sceneTestOutput = Invoke-CapturedNativeCommand -Executable $hython -Arguments @(
        $sceneFormatTestScript,
        $sceneOutputDirectory
    )
    if ($sceneTestOutput -match "OPUI_DialogPRM2|stepped on") {
        throw "Native scene-format test emitted duplicate-symbol warnings in Houdini $validationVersion"
    }
}

Write-Host "Native HouIO ROP matrix succeeded."
