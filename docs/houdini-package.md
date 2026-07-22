# Testing and installing the HouIO Houdini package

The Windows archive contains:

- `houio_convert.exe`
- The `houio_hom` Python bridge
- Shelf and Tab-menu tools
- Package diagnostics
- A transient bootstrap script
- An explicit persistent installer

HouIO supports Houdini 20.0 or newer.

Validated versions:

- Houdini 20.0.653
- Houdini 20.5.410
- Houdini 21.0.631
- Houdini 22.0.368

## Build the archive

```powershell
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release --target houio_houdini_package
```

Output:

```text
build/windows-msvc-release/houio-houdini-package-<version>-windows-x86_64.zip
```

## Bootstrap without installing

Use this workflow for manual evaluation. It does not copy package files into AppData and does not write loader files into any Houdini user folder.

Extract the archive:

```powershell
$Zip = "G:\Projects\Dev\Github\houio\build\windows-msvc-release\houio-houdini-package-0.2.0-windows-x86_64.zip"
$Extract = "G:\Projects\Dev\Github\houio\build\manual-package-test"

# Leave the extraction directory before attempting to delete it.
Set-Location (Split-Path -Parent $Extract)

if (Test-Path -LiteralPath $Extract) {
    Remove-Item -LiteralPath $Extract -Recurse -Force -ErrorAction Stop
}

Expand-Archive -LiteralPath $Zip -DestinationPath $Extract -Force
Set-Location $Extract
```

Launch a supported Houdini version with an isolated temporary package environment:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\bootstrap_houdini_package.ps1 `
  -HoudiniVersion 20.0
```

The script:

- Creates a temporary loader under the system temporary directory.
- Points that loader at the extracted archive.
- Uses a temporary `HOUDINI_USER_PREF_DIR`.
- Disables the user `houdini.env` file for that process.
- Launches Houdini with process-local environment variables.
- Waits for Houdini to close.
- Removes the temporary bootstrap directory after Houdini exits.

The extracted archive remains unchanged except when an explicit `-BootstrapDirectory` is supplied inside it.

Select another version:

```powershell
.\bootstrap_houdini_package.ps1 -HoudiniVersion 20.5
.\bootstrap_houdini_package.ps1 -HoudiniVersion 21.0
.\bootstrap_houdini_package.ps1 -HoudiniVersion 22.0
```

Use an exact executable when multiple builds are installed:

```powershell
.\bootstrap_houdini_package.ps1 `
  -HoudiniExecutable "C:\Program Files\Side Effects Software\Houdini 21.0.631\bin\houdini.exe"
```

Validate the bootstrap files without launching Houdini:

```powershell
.\bootstrap_houdini_package.ps1 -ValidateOnly
```

Preserve the temporary directory for inspection:

```powershell
.\bootstrap_houdini_package.ps1 `
  -HoudiniVersion 22.0 `
  -KeepBootstrap
```

The script prints the preserved path.

## Manual acceptance test

Inside the bootstrapped Houdini session:

1. Create a Geometry object and enter its SOP network.
2. Create and select a Box SOP.
3. Run **Tab > HouIO > Package Diagnostics**.
4. Confirm the package root, converter, and C-Blosc checks pass.
5. Run **Tab > HouIO > HouIO Round Trip**.
6. Confirm the created node cooks without errors or warnings.
7. Compare point and primitive counts with the source Box.
8. Disable **Enabled** and confirm geometry passes through unchanged.
9. Use **Convert Geometry File** to write `.bgeo.sc`.
10. Load the result with a File SOP.

Repeat the test in each Houdini version you intend to support.

## Persistent installation

Persistent installation is separate from bootstrap testing and requires an explicit action flag:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\install_houdini_package.ps1 `
  -Install
```

The installer:

- Copies package files to `%LOCALAPPDATA%\HouIO`.
- Creates one loader per selected Houdini version in the Windows Documents folder.
- Targets 20.0, 20.5, 21.0, and 22.0 by default.

Install one version:

```powershell
.\install_houdini_package.ps1 `
  -Install `
  -HoudiniVersions 20.0
```

Running the installer with no `-Install` or `-Uninstall` action stops without changing the system.

## Houdini 22 Package Browser

Houdini 22 can install the ZIP directly through **Inspectors > Package Browser**. This is a persistent installation and is not the transient bootstrap workflow.

## Tools

Open a SOP network and press **Tab > HouIO**.

### HouIO Round Trip

Creates a configured Python SOP after the selected SOP.

Parameters:

- **Enabled** bypasses HouIO when disabled.
- **Timeout (seconds)** limits converter execution; default `300`.

### Convert Geometry File

Converts `.geo`, `.bgeo`, or `.bgeo.sc` through the bundled executable.

### Package Diagnostics

Reports the active package root, Houdini version, converter path, C-Blosc path, and runtime existence checks.

## Supported data

The package supports HouIO's polygon, numeric/string/dictionary attribute, group, and dense scalar-volume model. Houdini Volume Visualization detail metadata is preserved in both the scalar-attribute layout used by Houdini 20.x and the dictionary layout used by Houdini 21.x and newer.

The Houdini bridge also accepts bounded 32-bit Float SDF and Fog VDB grids by converting them to dense volumes. VDB class is retained through the `houio_vdb_class` attribute and restored on output.

Unsupported examples include packed primitives, agents, height fields, vector VDB grids, and native sparse VDB preservation.

## Runtime model

The package does not load a HouIO extension into Houdini. It imports Python code and starts `houio_convert.exe` as a separate process.

`.bgeo.sc` support resolves C-Blosc from the active Houdini installation through `$HFS/bin/blosc.dll`.

## Uninstall

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  "$env:LOCALAPPDATA\HouIO\install_houdini_package.ps1" `
  -Uninstall
```

Keep package files while removing loaders:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  "$env:LOCALAPPDATA\HouIO\install_houdini_package.ps1" `
  -Uninstall `
  -KeepFiles
```

## Development setup

For source-tree development, use `tools/houdini/install_hom_bridge.ps1` and follow [Houdini integration on Windows](houdini-windows.md).
