[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet("help", "build", "test", "fixtures", "package", "native-rop", "scene-deps", "scene-package", "benchmarks", "validate-all")]
    [string]$Command = "help",

    [string]$Preset = "windows-msvc-release",

    [string]$HoudiniVersion = "21.0.631",

    [switch]$SkipHoudini,

    [switch]$ConfirmLargeDownload
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$visualStudioShell = ""
$visualStudioCMake = ""
$visualStudioCTest = ""

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

function Initialize-Toolchain {
    if (-not (Test-Path -LiteralPath $visualStudioShell -PathType Leaf)) {
        throw "Visual Studio Developer PowerShell was not found: $visualStudioShell"
    }
    & $visualStudioShell -Arch amd64 -HostArch amd64
}

function Resolve-BuildTool {
    param(
        [Parameter(Mandatory)]
        [string]$PreferredPath,

        [Parameter(Mandatory)]
        [string]$CommandName
    )

    if (Test-Path -LiteralPath $PreferredPath -PathType Leaf) {
        return $PreferredPath
    }
    return (Get-Command $CommandName -ErrorAction Stop).Source
}

function Get-HythonPath {
    param(
        [Parameter(Mandatory)]
        [string]$Version
    )

    $path = "C:\Program Files\Side Effects Software\Houdini $Version\bin\hython.exe"
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Houdini $Version hython was not found: $path"
    }
    return $path
}

function Initialize-Preset {
    param(
        [Parameter(Mandatory)]
        [string]$Name,

        [string]$Hython = ""
    )

    $arguments = @("--preset", $Name)
    if ($Hython) {
        $arguments += "-DHOUIO_HYTHON_EXECUTABLE=$Hython"
    }
    Invoke-NativeCommand -Executable $script:cmake -Arguments $arguments
}

function Build-Preset {
    param(
        [Parameter(Mandatory)]
        [string]$Name,

        [string[]]$Targets = @()
    )

    $arguments = @("--build", "--preset", $Name)
    if ($Targets.Count -gt 0) {
        $arguments += "--target"
        $arguments += $Targets
    }
    Invoke-NativeCommand -Executable $script:cmake -Arguments $arguments
}

function Test-Preset {
    param(
        [Parameter(Mandatory)]
        [string]$Name
    )

    Invoke-NativeCommand -Executable $script:ctest -Arguments @(
        "--preset",
        $Name,
        "--output-on-failure"
    )
}

function Show-Help {
    Write-Host "HouIO developer workflow"
    Write-Host ""
    Write-Host "Usage:"
    Write-Host "  .\tools\dev.ps1 build [-Preset windows-msvc-release]"
    Write-Host "  .\tools\dev.ps1 test [-Preset windows-msvc-release] [-HoudiniVersion 21.0.631]"
    Write-Host "  .\tools\dev.ps1 fixtures"
    Write-Host "  .\tools\dev.ps1 package [-HoudiniVersion 21.0.631]"
    Write-Host "  .\tools\dev.ps1 native-rop"
    Write-Host "  .\tools\dev.ps1 scene-deps -ConfirmLargeDownload"
    Write-Host "  .\tools\dev.ps1 scene-package"
    Write-Host "  .\tools\dev.ps1 benchmarks"
    Write-Host "  .\tools\dev.ps1 validate-all [-SkipHoudini]"
    Write-Host ""
    Write-Host "validate-all runs warnings-as-errors, static analysis, AddressSanitizer,"
    Write-Host "and, unless -SkipHoudini is supplied, the four-version fixture, package, and native ROP matrices."
}

if ($Command -eq "help") {
    Show-Help
    exit 0
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
    throw "Visual Studio Installer discovery tool was not found: $vswhere"
}
$visualStudioRoot = (& $vswhere `
    -latest `
    -products "*" `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath).Trim()
if (-not $visualStudioRoot) {
    throw "No Visual Studio installation with the MSVC x64 toolchain was found"
}
$visualStudioShell = Join-Path $visualStudioRoot "Common7\Tools\Launch-VsDevShell.ps1"
$kitwareCMakeDirectory = "C:\Program Files\CMake\bin"
$visualStudioCMakeDirectory = Join-Path $visualStudioRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
if (Test-Path -LiteralPath (Join-Path $kitwareCMakeDirectory "cmake.exe") -PathType Leaf) {
    $preferredCMakeDirectory = $kitwareCMakeDirectory
}
else {
    $preferredCMakeDirectory = $visualStudioCMakeDirectory
}
$visualStudioCMake = Join-Path $preferredCMakeDirectory "cmake.exe"
$visualStudioCTest = Join-Path $preferredCMakeDirectory "ctest.exe"

Initialize-Toolchain
Set-Location $repositoryRoot
$script:cmake = Resolve-BuildTool -PreferredPath $visualStudioCMake -CommandName "cmake"
$script:ctest = Resolve-BuildTool -PreferredPath $visualStudioCTest -CommandName "ctest"

switch ($Command) {
    "build" {
        Initialize-Preset -Name $Preset
        Build-Preset -Name $Preset
    }
    "test" {
        $hython = Get-HythonPath -Version $HoudiniVersion
        Initialize-Preset -Name $Preset -Hython $hython
        Build-Preset -Name $Preset
        Test-Preset -Name $Preset
    }
    "fixtures" {
        Invoke-NativeCommand -Executable "powershell.exe" -Arguments @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            (Join-Path $repositoryRoot "tools\houdini\run_fixture_roundtrips.ps1")
        )
    }
    "package" {
        $hython = Get-HythonPath -Version $HoudiniVersion
        Initialize-Preset -Name "windows-msvc-release" -Hython $hython
        Build-Preset -Name "windows-msvc-release" -Targets @("houio_houdini_package")
    }
    "native-rop" {
        Invoke-NativeCommand -Executable "powershell.exe" -Arguments @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            (Join-Path $repositoryRoot "tools\houdini\run_native_rop_matrix.ps1")
        )
    }
    "scene-deps" {
        $arguments = @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            (Join-Path $repositoryRoot "tools\dependencies\build_scene_io.ps1")
        )
        if ($ConfirmLargeDownload) {
            $arguments += "-ConfirmLargeDownload"
        }
        Invoke-NativeCommand -Executable "powershell.exe" -Arguments $arguments
    }
    "scene-package" {
        $dependencyRoot = Join-Path $repositoryRoot "build\dependencies\scene-io\install"
        $dependencyManifest = Join-Path $dependencyRoot "houio-scene-dependencies.json"
        if (-not (Test-Path -LiteralPath $dependencyManifest -PathType Leaf)) {
            throw "Bundled scene dependencies are not ready. Run .\tools\dev.ps1 scene-deps -ConfirmLargeDownload first."
        }
        $env:HOUIO_BUNDLED_SCENE_IO_ROOT = $dependencyRoot
        $env:PATH = "$(Join-Path $dependencyRoot 'bin');$env:PATH"
        Initialize-Preset -Name "windows-msvc-release-bundled"
        Build-Preset -Name "windows-msvc-release-bundled"
        Test-Preset -Name "windows-msvc-release-bundled"
        Build-Preset -Name "windows-msvc-release-bundled" -Targets @("houio_portable_package")
    }
    "benchmarks" {
        Initialize-Preset -Name "windows-msvc-benchmarks"
        Build-Preset -Name "windows-msvc-benchmarks"
        Invoke-NativeCommand -Executable (Join-Path $repositoryRoot "build\windows-msvc-benchmarks\houio_benchmarks.exe") -Arguments @()
        Invoke-NativeCommand -Executable (Join-Path $repositoryRoot "build\windows-msvc-benchmarks\houio_memory_probe.exe") -Arguments @()
    }
    "validate-all" {
        Initialize-Preset -Name "windows-msvc-werror"
        Build-Preset -Name "windows-msvc-werror"
        Test-Preset -Name "windows-msvc-werror"

        Initialize-Preset -Name "windows-msvc-analysis"
        Build-Preset -Name "windows-msvc-analysis"

        Initialize-Preset -Name "windows-msvc-asan"
        Build-Preset -Name "windows-msvc-asan"
        Test-Preset -Name "windows-msvc-asan"

        if (-not $SkipHoudini) {
            Invoke-NativeCommand -Executable "powershell.exe" -Arguments @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                (Join-Path $repositoryRoot "tools\houdini\run_fixture_roundtrips.ps1")
            )
            Invoke-NativeCommand -Executable "powershell.exe" -Arguments @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                (Join-Path $repositoryRoot "tools\houdini\run_package_matrix.ps1")
            )
            Invoke-NativeCommand -Executable "powershell.exe" -Arguments @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                (Join-Path $repositoryRoot "tools\houdini\run_native_rop_matrix.ps1")
            )
        }
    }
}
