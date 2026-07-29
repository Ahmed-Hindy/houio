[CmdletBinding()]
param(
    [string]$Root,
    [switch]$Clean,
    [switch]$ConfirmLargeDownload
)

$ErrorActionPreference = "Stop"
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if (-not $Root) {
    $Root = Join-Path $repositoryRoot "build\dependencies\scene-io"
}
$Root = [System.IO.Path]::GetFullPath($Root)

if (-not $ConfirmLargeDownload) {
    throw @"
Building HouIO's bundled scene dependencies downloads and compiles OpenUSD,
Alembic, Imath, oneTBB, and related OpenUSD prerequisites. This is a large,
long-running operation. Re-run with -ConfirmLargeDownload after approving it.
"@
}

$versionFile = Join-Path $repositoryRoot "cmake\scene-io-versions.cmake"
$versionText = Get-Content -LiteralPath $versionFile -Raw
function Get-PinnedVersion([string]$name) {
    $match = [regex]::Match(
        $versionText,
        "set\($name\s+`"([^`"]+)`"\)")
    if (-not $match.Success) {
        throw "Could not read $name from $versionFile"
    }
    return $match.Groups[1].Value
}

$alembicVersion = Get-PinnedVersion "HOUIO_ALEMBIC_VERSION"
$alembicRevision = Get-PinnedVersion "HOUIO_ALEMBIC_REVISION"
$imathVersion = Get-PinnedVersion "HOUIO_IMATH_VERSION"
$imathRevision = Get-PinnedVersion "HOUIO_IMATH_REVISION"
$openUsdVersion = Get-PinnedVersion "HOUIO_OPENUSD_VERSION"
$openUsdRevision = Get-PinnedVersion "HOUIO_OPENUSD_REVISION"
$oneTbbVersion = Get-PinnedVersion "HOUIO_ONETBB_VERSION"

$sourceRoot = Join-Path $Root "src"
$buildRoot = Join-Path $Root "build"
$installRoot = Join-Path $Root "install"
if ($Clean -and (Test-Path -LiteralPath $Root)) {
    Remove-Item -LiteralPath $Root -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $sourceRoot, $buildRoot, $installRoot | Out-Null

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
    throw "Visual Studio Installer discovery tool was not found: $vswhere"
}
$visualStudioRoot = (& $vswhere `
    -latest `
    -products "*" `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath).Trim()
$visualStudioVersion = (& $vswhere `
    -latest `
    -products "*" `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationVersion).Trim()
if (-not $visualStudioRoot -or -not $visualStudioVersion) {
    throw "No Visual Studio installation with the MSVC x64 toolchain was found"
}
$vsDevShell = Join-Path $visualStudioRoot "Common7\Tools\Launch-VsDevShell.ps1"
if (-not (Test-Path -LiteralPath $vsDevShell -PathType Leaf)) {
    throw "Visual Studio Developer PowerShell was not found: $vsDevShell"
}
& $vsDevShell -Arch amd64 -HostArch amd64

$cmake = "C:\Program Files\CMake\bin\cmake.exe"
if (-not (Test-Path -LiteralPath $cmake -PathType Leaf)) {
    $cmake = (Get-Command cmake -ErrorAction Stop).Source
}
$cmakeVersionText = (& $cmake --version | Select-Object -First 1) -replace "^cmake version ", ""
if ([version]$cmakeVersionText -lt [version]"3.29") {
    throw "CMake 3.29 or newer is required; found $cmakeVersionText"
}
$git = (Get-Command git -ErrorAction Stop).Source
$uv = (Get-Command uv -ErrorAction Stop).Source
$ninja = Join-Path $visualStudioRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if (-not (Test-Path -LiteralPath $ninja -PathType Leaf)) {
    throw "Visual Studio's bundled Ninja executable was not found: $ninja"
}
$env:PATH = "$(Split-Path -Parent $cmake);$(Split-Path -Parent $ninja);$env:PATH"

function Invoke-Native([string]$executable, [string[]]$arguments) {
    Write-Host "> $executable $($arguments -join ' ')"
    & $executable @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $executable"
    }
}

function Get-GitRevision([string]$sourceDirectory) {
    $revision = (& $git -C $sourceDirectory rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or -not $revision) {
        throw "Could not resolve the Git revision for $sourceDirectory"
    }
    return $revision
}

function Get-Sha256([string]$path) {
    $stream = [System.IO.File]::OpenRead($path)
    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = $algorithm.ComputeHash($stream)
        return ([System.BitConverter]::ToString($bytes) -replace "-", "").ToLowerInvariant()
    }
    finally {
        $algorithm.Dispose()
        $stream.Dispose()
    }
}

function Get-ShallowSource(
    [string]$name,
    [string]$repository,
    [string]$tag,
    [string]$expectedRevision
) {
    $destination = Join-Path $sourceRoot $name
    if (Test-Path -LiteralPath $destination -PathType Container) {
        $currentRevision = Get-GitRevision $destination
        if ($currentRevision -ne $expectedRevision) {
            Remove-Item -LiteralPath $destination -Recurse -Force
        }
    }
    if (-not (Test-Path -LiteralPath $destination -PathType Container)) {
        Invoke-Native $git @(
            "clone",
            "--depth", "1",
            "--branch", $tag,
            "--recurse-submodules",
            "--shallow-submodules",
            $repository,
            $destination
        )
    }
    $actualRevision = Get-GitRevision $destination
    if ($actualRevision -ne $expectedRevision) {
        throw "Pinned source mismatch for ${name}: expected $expectedRevision, found $actualRevision"
    }
    return $destination
}

$openUsdSource = Get-ShallowSource `
    "OpenUSD-$openUsdVersion" `
    "https://github.com/PixarAnimationStudios/OpenUSD.git" `
    "v$openUsdVersion" `
    $openUsdRevision

# OpenUSD's official build script downloads and builds its required TBB/zlib
# dependencies into the same relocatable prefix. Python, imaging, tools, docs,
# examples, tutorials, MaterialX, validation, and the Alembic plugin are not
# needed by HouIO's writer backend.
Invoke-Native $uv @(
    "run", "--python", "3.13", "python",
    (Join-Path $openUsdSource "build_scripts\build_usd.py"),
    "--build-shared",
    "--generator", "Ninja",
    "--no-python",
    "--no-imaging",
    "--no-usdview",
    "--no-examples",
    "--no-tutorials",
    "--no-tools",
    "--no-docs",
    "--no-python-docs",
    "--no-usdValidation",
    "--no-materialx",
    "--no-alembic",
    "--onetbb",
    "--build", (Join-Path $buildRoot "OpenUSD"),
    "--src", (Join-Path $sourceRoot "OpenUSD-dependencies"),
    $installRoot
)

$imathSource = Get-ShallowSource `
    "Imath-$imathVersion" `
    "https://github.com/AcademySoftwareFoundation/Imath.git" `
    "v$imathVersion" `
    $imathRevision
$imathBuild = Join-Path $buildRoot "Imath"
Invoke-Native $cmake @(
    "-S", $imathSource,
    "-B", $imathBuild,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_INSTALL_PREFIX=$installRoot",
    "-DBUILD_SHARED_LIBS=ON",
    "-DBUILD_TESTING=OFF"
)
Invoke-Native $cmake @(
    "--build", $imathBuild,
    "--target", "install",
    "--parallel"
)

$alembicSource = Get-ShallowSource `
    "Alembic-$alembicVersion" `
    "https://github.com/alembic/alembic.git" `
    $alembicVersion `
    $alembicRevision
$alembicBuild = Join-Path $buildRoot "Alembic"
Invoke-Native $cmake @(
    "-S", $alembicSource,
    "-B", $alembicBuild,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_INSTALL_PREFIX=$installRoot",
    "-DCMAKE_PREFIX_PATH=$installRoot",
    "-DALEMBIC_SHARED_LIBS=ON",
    "-DUSE_HDF5=OFF",
    "-DUSE_PYALEMBIC=OFF",
    "-DUSE_TESTS=OFF"
)
Invoke-Native $cmake @(
    "--build", $alembicBuild,
    "--target", "install",
    "--parallel"
)

# OpenUSD installs its Windows runtime DLLs in lib while Windows resolves
# transitive dependencies most reliably beside the executable. Preserve the
# import-library/resource layout and copy runtime DLLs into bin for portable
# HouIO packages.
$runtimeBin = Join-Path $installRoot "bin"
New-Item -ItemType Directory -Force -Path $runtimeBin | Out-Null
$usdRuntimeDlls = Get-ChildItem -LiteralPath (Join-Path $installRoot "lib") `
    -Filter "*.dll" `
    -File `
    -ErrorAction Stop
if (-not $usdRuntimeDlls) {
    throw "OpenUSD installed no runtime DLLs under $installRoot\lib"
}
foreach ($runtimeDll in $usdRuntimeDlls) {
    Copy-Item -LiteralPath $runtimeDll.FullName `
        -Destination (Join-Path $runtimeBin $runtimeDll.Name) `
        -Force
}

$usdResourceSource = Join-Path $installRoot "lib\usd"
$usdResourceDestination = Join-Path $runtimeBin "usd"
if (-not (Test-Path -LiteralPath $usdResourceSource -PathType Container)) {
    throw "OpenUSD installed no plugin resource tree under $usdResourceSource"
}
Remove-Item -LiteralPath $usdResourceDestination -Recurse -Force `
    -ErrorAction SilentlyContinue
Copy-Item -LiteralPath $usdResourceSource `
    -Destination $usdResourceDestination `
    -Recurse `
    -Force

$runtimeHashes = [ordered]@{}
Get-ChildItem -LiteralPath $runtimeBin -Filter "*.dll" -File |
    Sort-Object Name |
    ForEach-Object {
        $runtimeHashes[$_.Name] = Get-Sha256 $_.FullName
    }

$licenseRoot = Join-Path $installRoot "share\houio\licenses"
New-Item -ItemType Directory -Force -Path $licenseRoot | Out-Null
function Copy-LicenseFiles(
    [string]$projectName,
    [string]$projectRoot,
    [string[]]$fileNames
) {
    foreach ($fileName in $fileNames) {
        $source = Join-Path $projectRoot $fileName
        if (Test-Path -LiteralPath $source -PathType Leaf) {
            $safeName = $fileName -replace "[\\/]", "_"
            Copy-Item -LiteralPath $source `
                -Destination (Join-Path $licenseRoot "$projectName-$safeName") `
                -Force
        }
    }
}

Copy-LicenseFiles "OpenUSD" $openUsdSource @("LICENSE.txt", "NOTICE.txt")
$oneTbbSource = Join-Path $sourceRoot "OpenUSD-dependencies\oneTBB-$oneTbbVersion"
if (-not (Test-Path -LiteralPath $oneTbbSource -PathType Container)) {
    throw "OpenUSD did not provide the pinned oneTBB source directory: $oneTbbSource"
}
Copy-LicenseFiles "oneTBB" $oneTbbSource @("LICENSE.txt")
Copy-LicenseFiles "Alembic" $alembicSource @(
    "LICENSE.txt",
    "THIRD-PARTY.txt",
    "ACKNOWLEDGEMENTS.txt"
)
Copy-LicenseFiles "Imath" $imathSource @("LICENSE.md", "LICENSE.txt")

$manifest = [ordered]@{
    schema = "houio.scene-dependencies/1"
    versions = [ordered]@{
        alembic = $alembicVersion
        imath = $imathVersion
        openusd = $openUsdVersion
        onetbb = $oneTbbVersion
    }
    sources = [ordered]@{
        alembic = [ordered]@{
            repository = "https://github.com/alembic/alembic"
            revision = Get-GitRevision $alembicSource
        }
        imath = [ordered]@{
            repository = "https://github.com/AcademySoftwareFoundation/Imath"
            revision = Get-GitRevision $imathSource
        }
        openusd = [ordered]@{
            repository = "https://github.com/PixarAnimationStudios/OpenUSD"
            revision = Get-GitRevision $openUsdSource
        }
        onetbb = [ordered]@{
            repository = "https://github.com/uxlfoundation/oneTBB"
            version = $oneTbbVersion
        }
    }
    toolchain = [ordered]@{
        cmake = $cmakeVersionText
        generator = "Ninja"
        visual_studio = $visualStudioVersion
    }
    runtime_sha256 = $runtimeHashes
    layout = "relocatable-prefix"
}
$manifest | ConvertTo-Json | Set-Content `
    -LiteralPath (Join-Path $installRoot "houio-scene-dependencies.json") `
    -Encoding utf8

Write-Host "Bundled scene dependency prefix: $installRoot"
Write-Host "Configure HouIO with:"
Write-Host "  -DHOUIO_SCENE_IO_PROVIDER=bundled"
Write-Host "  -DHOUIO_BUNDLED_SCENE_IO_ROOT=$installRoot"
