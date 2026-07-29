[CmdletBinding()]
param(
    [string]$PackageRoot,
    [string]$ManifestPath,
    [string]$WorkDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if (-not $PackageRoot) {
    $PackageRoot = Join-Path $repositoryRoot "build\windows-msvc-bundled-scene-io\install"
}
if (-not $ManifestPath) {
    $ManifestPath = Join-Path $repositoryRoot "tests\data\scene_writer_manifest.json"
}
if (-not $WorkDirectory) {
    $WorkDirectory = Join-Path $repositoryRoot "build\bundled-scene-io-validation"
}

$PackageRoot = [System.IO.Path]::GetFullPath($PackageRoot)
$ManifestPath = [System.IO.Path]::GetFullPath($ManifestPath)
$WorkDirectory = [System.IO.Path]::GetFullPath($WorkDirectory)
$cli = Join-Path $PackageRoot "bin\houio.exe"

if (-not (Test-Path -LiteralPath $cli -PathType Leaf)) {
    throw "Packaged HouIO CLI was not found: $cli"
}
if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
    throw "Scene-writer manifest was not found: $ManifestPath"
}

function Invoke-Native(
    [Parameter(Mandatory)]
    [string]$Executable,
    [Parameter(Mandatory)]
    [string[]]$Arguments
) {
    $output = @(& $Executable @Arguments 2>&1)
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        throw @"
Command failed with exit code ${exitCode}:
  $Executable $($Arguments -join ' ')
$($output -join "`n")
"@
    }
    return ($output -join "`n").Trim()
}

function Get-Sha256(
    [Parameter(Mandatory)]
    [string]$Path
) {
    $stream = [System.IO.File]::OpenRead($Path)
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

function Require-SceneCapability(
    [Parameter(Mandatory)]
    [object]$Capabilities,
    [Parameter(Mandatory)]
    [string]$Name
) {
    $capability = $Capabilities.capabilities |
        Where-Object { $_.name -eq $Name } |
        Select-Object -First 1
    if ($null -eq $capability) {
        throw "Packaged HouIO did not report the $Name capability"
    }
    if ($capability.level -ne "supported" -or -not $capability.writable) {
        throw "Packaged HouIO reported $Name as unavailable or read-only"
    }
}

function Resolve-Dumpbin {
    $searchPath = "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe"
    $candidates = Get-ChildItem -Path $searchPath -File -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending
    if (-not $candidates) {
        throw "dumpbin.exe was not found under Visual Studio 2022"
    }
    return $candidates[0].FullName
}

function Read-And-ValidateDependencyMetadata(
    [Parameter(Mandatory)]
    [string]$Root
) {
    $licenseRoot = Join-Path $Root "share\houio\licenses"
    $dependencyManifestPath = Join-Path $licenseRoot "houio-scene-dependencies.json"
    if (-not (Test-Path -LiteralPath $dependencyManifestPath -PathType Leaf)) {
        throw "Bundled dependency manifest is missing: $dependencyManifestPath"
    }

    $requiredLicenseFiles = @(
        "OpenUSD-LICENSE.txt",
        "OpenUSD-NOTICE.txt",
        "Alembic-LICENSE.txt",
        "Alembic-THIRD-PARTY.txt",
        "Alembic-ACKNOWLEDGEMENTS.txt",
        "Imath-LICENSE.md",
        "oneTBB-LICENSE.txt"
    )
    foreach ($licenseFile in $requiredLicenseFiles) {
        $path = Join-Path $licenseRoot $licenseFile
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required third-party notice is missing: $path"
        }
    }

    $manifest = Get-Content -LiteralPath $dependencyManifestPath -Raw |
        ConvertFrom-Json
    if ($manifest.schema -ne "houio.scene-dependencies/1") {
        throw "Unsupported bundled dependency manifest schema: $($manifest.schema)"
    }
    if ($null -eq $manifest.runtime_sha256) {
        throw "Bundled dependency manifest contains no runtime SHA-256 map"
    }

    $verifiedHashes = 0
    foreach ($property in $manifest.runtime_sha256.PSObject.Properties) {
        $runtimePath = Join-Path (Join-Path $Root "bin") $property.Name
        if (-not (Test-Path -LiteralPath $runtimePath -PathType Leaf)) {
            throw "Manifest runtime DLL is missing from the package: $runtimePath"
        }
        $actual = Get-Sha256 $runtimePath
        $expected = [string]$property.Value
        if ($actual -ne $expected.ToLowerInvariant()) {
            throw "Runtime DLL hash mismatch for $($property.Name)"
        }
        ++$verifiedHashes
    }
    if ($verifiedHashes -eq 0) {
        throw "Bundled dependency manifest contains an empty runtime SHA-256 map"
    }

    return [ordered]@{
        manifest = $dependencyManifestPath
        licenses = $requiredLicenseFiles
        versions = $manifest.versions
        runtime_hashes_verified = $verifiedHashes
    }
}

function Assert-NoSideFxImports(
    [Parameter(Mandatory)]
    [string]$Root
) {
    $dumpbin = Resolve-Dumpbin
    $forbidden = "(?i)(houdini|sidefx|alembic_sidefx|libpxr_|hboost_|python3[0-9]*\.dll)"
    $binaries = Get-ChildItem -LiteralPath $Root -Recurse -File |
        Where-Object { $_.Extension -in @(".exe", ".dll") }
    if (-not $binaries) {
        throw "The package contains no executable or DLL files"
    }

    foreach ($binary in $binaries) {
        if ($binary.Name -match $forbidden) {
            throw "Forbidden SideFX/Houdini-style runtime file in package: $($binary.FullName)"
        }
        $dependencies = @(& $dumpbin /nologo /dependents $binary.FullName 2>&1)
        if ($LASTEXITCODE -ne 0) {
            throw "dumpbin failed for $($binary.FullName): $($dependencies -join "`n")"
        }
        $matched = $dependencies | Where-Object { $_ -match $forbidden }
        if ($matched) {
            throw @"
$($binary.FullName) imports a forbidden SideFX/Houdini-style dependency:
$($matched -join "`n")
"@
        }
    }
}

Remove-Item -LiteralPath $WorkDirectory -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $WorkDirectory | Out-Null

$environmentNames = @(
    "HFS",
    "HSITE",
    "HOUDINI_DSO_PATH",
    "HOUDINI_PATH",
    "HOUDINI_PACKAGE_DIR",
    "HOUDINI_USER_PREF_DIR",
    "HOUIO_BLOSC_LIBRARY",
    "PXR_PLUGINPATH_NAME",
    "PYTHONHOME",
    "PYTHONPATH"
)
$originalEnvironment = @{}
foreach ($name in $environmentNames) {
    $originalEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, "Process")
    [Environment]::SetEnvironmentVariable($name, $null, "Process")
}
$originalPath = $env:PATH

try {
    $cleanPath = @(
        (Join-Path $PackageRoot "bin"),
        $env:SystemRoot,
        (Join-Path $env:SystemRoot "System32"),
        (Join-Path $env:SystemRoot "System32\Wbem")
    ) -join ";"
    $env:PATH = $cleanPath
    if ($env:PATH -match "(?i)(Side Effects Software|Houdini)") {
        throw "The scrubbed runtime PATH still contains a Houdini directory"
    }

    $capabilities = Invoke-Native $cli @("capabilities", "--json") |
        ConvertFrom-Json
    Require-SceneCapability $capabilities "alembic_scene"
    Require-SceneCapability $capabilities "usd_scene"

    $fixture = Join-Path $WorkDirectory "scene_writer_fixture.bgeo"
    $writeResult = Invoke-Native $cli @(
        "write-manifest",
        $ManifestPath,
        $fixture,
        "--json"
    ) | ConvertFrom-Json
    if (-not $writeResult.success -or -not (Test-Path -LiteralPath $fixture -PathType Leaf)) {
        throw "The packaged CLI could not create its BGEO fixture"
    }

    $outputs = [ordered]@{}
    foreach ($extension in @("abc", "usda", "usdc")) {
        $output = Join-Path $WorkDirectory "scene_writer_fixture.$extension"
        $conversion = Invoke-Native $cli @(
            "convert",
            $fixture,
            $output,
            "--json"
        ) | ConvertFrom-Json
        if (-not $conversion.success) {
            throw "Packaged HouIO conversion failed for .$extension"
        }
        if (-not (Test-Path -LiteralPath $output -PathType Leaf)) {
            throw "Packaged HouIO did not create $output"
        }
        $size = (Get-Item -LiteralPath $output).Length
        if ($size -le 0) {
            throw "Packaged HouIO created an empty .$extension file"
        }
        $outputs[$extension] = [ordered]@{
            path = $output
            bytes = $size
        }
    }

    $dependencyMetadata = Read-And-ValidateDependencyMetadata $PackageRoot
    Assert-NoSideFxImports $PackageRoot

    [ordered]@{
        schema = "houio.bundled-scene-validation/1"
        status = "success"
        package_root = $PackageRoot
        cli = $cli
        path = $env:PATH
        sidefx_environment_removed = $true
        alembic_supported = $true
        usd_supported = $true
        fixture = $fixture
        outputs = $outputs
        dependency_metadata = $dependencyMetadata
        sidefx_import_audit = "passed"
        runtime_hash_audit = "passed"
    } | ConvertTo-Json -Depth 6
}
finally {
    $env:PATH = $originalPath
    foreach ($name in $environmentNames) {
        [Environment]::SetEnvironmentVariable(
            $name,
            $originalEnvironment[$name],
            "Process"
        )
    }
}
