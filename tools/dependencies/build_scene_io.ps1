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
$imathVersion = Get-PinnedVersion "HOUIO_IMATH_VERSION"
$openUsdVersion = Get-PinnedVersion "HOUIO_OPENUSD_VERSION"

$sourceRoot = Join-Path $Root "src"
$buildRoot = Join-Path $Root "build"
$installRoot = Join-Path $Root "install"
if ($Clean -and (Test-Path -LiteralPath $Root)) {
    Remove-Item -LiteralPath $Root -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $sourceRoot, $buildRoot, $installRoot | Out-Null

$vsDevShell = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1"
if (-not (Test-Path -LiteralPath $vsDevShell -PathType Leaf)) {
    throw "Visual Studio Developer PowerShell was not found: $vsDevShell"
}
& $vsDevShell -Arch amd64 -HostArch amd64

$cmake = (Get-Command cmake -ErrorAction Stop).Source
$git = (Get-Command git -ErrorAction Stop).Source
$uv = (Get-Command uv -ErrorAction Stop).Source

function Invoke-Native([string]$executable, [string[]]$arguments) {
    Write-Host "> $executable $($arguments -join ' ')"
    & $executable @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $executable"
    }
}

function Get-ShallowSource(
    [string]$name,
    [string]$repository,
    [string]$tag
) {
    $destination = Join-Path $sourceRoot $name
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
    return $destination
}

$openUsdSource = Get-ShallowSource `
    "OpenUSD-$openUsdVersion" `
    "https://github.com/PixarAnimationStudios/OpenUSD.git" `
    "v$openUsdVersion"

# OpenUSD's official build script downloads and builds its required TBB/zlib
# dependencies into the same relocatable prefix. Python, imaging, tools, docs,
# examples, tutorials, MaterialX, validation, and the Alembic plugin are not
# needed by HouIO's writer backend.
Invoke-Native $uv @(
    "run", "--python", "3.13", "python",
    (Join-Path $openUsdSource "build_scripts\build_usd.py"),
    "--build-shared",
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
    "v$imathVersion"
$imathBuild = Join-Path $buildRoot "Imath"
Invoke-Native $cmake @(
    "-S", $imathSource,
    "-B", $imathBuild,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_INSTALL_PREFIX=$installRoot",
    "-DBUILD_SHARED_LIBS=ON",
    "-DIMATH_BUILD_TESTS=OFF",
    "-DIMATH_BUILD_EXAMPLES=OFF",
    "-DIMATH_BUILD_PYTHON=OFF"
)
Invoke-Native $cmake @(
    "--build", $imathBuild,
    "--target", "install",
    "--parallel"
)

$alembicSource = Get-ShallowSource `
    "Alembic-$alembicVersion" `
    "https://github.com/alembic/alembic.git" `
    $alembicVersion
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
Copy-LicenseFiles "Alembic" $alembicSource @(
    "LICENSE.txt",
    "THIRD-PARTY.txt",
    "ACKNOWLEDGEMENTS.txt"
)
Copy-LicenseFiles "Imath" $imathSource @("LICENSE.md", "LICENSE.txt")

$manifest = [ordered]@{
    schema = "houio.scene-dependencies/1"
    alembic = $alembicVersion
    imath = $imathVersion
    openusd = $openUsdVersion
    sources = [ordered]@{
        alembic = "https://github.com/alembic/alembic"
        imath = "https://github.com/AcademySoftwareFoundation/Imath"
        openusd = "https://github.com/PixarAnimationStudios/OpenUSD"
    }
    install_prefix = $installRoot
    generated_utc = [DateTime]::UtcNow.ToString("o")
}
$manifest | ConvertTo-Json | Set-Content `
    -LiteralPath (Join-Path $installRoot "houio-scene-dependencies.json") `
    -Encoding utf8

Write-Host "Bundled scene dependency prefix: $installRoot"
Write-Host "Configure HouIO with:"
Write-Host "  -DHOUIO_SCENE_IO_PROVIDER=bundled"
Write-Host "  -DHOUIO_BUNDLED_SCENE_IO_ROOT=$installRoot"
