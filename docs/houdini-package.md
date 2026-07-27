# Testing and installing the HouIO Houdini package

The Windows archive contains:

- `houio.exe`, the primary custom-writer CLI
- `houio_convert.exe`, the compatibility converter
- The `houio_hom` direct HOM extraction bridge
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
4. Confirm the package root, writer, converter, and C-Blosc checks pass.
5. Run **Tab > HouIO > Write Selected Geometry** and choose a `.bgeo.sc` destination.
6. Load the result with a File SOP and compare attributes, groups, points, and primitives.
7. Run **Tab > HouIO > HouIO Round Trip**.
8. Confirm the created node cooks without errors or warnings.
9. Disable **Enabled** and confirm geometry passes through unchanged.
10. Use **Convert Geometry File** for compatibility file-to-file conversion.

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

### Write Selected Geometry

Writes the selected cooked SOP through direct HOM extraction and HouIO's custom C++ serializer. The primary path does not call `hou.Geometry.data()` or `hou.Geometry.saveToFile()`.

Supported live records include polygons, dense scalar volumes, embedded `hou.PackedGeometry`, external `PackedDisk`, and `PackedDiskSequence` references, together with maintained point, vertex, primitive, and global attributes and groups.

### HouIO Round Trip

Creates a configured Python SOP after the selected SOP.

Parameters:

- **Enabled** bypasses HouIO when disabled.
- **Timeout (seconds)** limits converter execution; default `300`.

Supported VDB inputs are densified only for the external HouIO process and are restored as VDB primitives when the node finishes cooking. Native dense volumes remain dense volumes.

### Convert Geometry File

Converts `.geo`, `.bgeo`, or `.bgeo.sc` through the bundled executable.

### Package Diagnostics

Reports the active package root, Houdini version, primary writer path and `houio diagnose --json` result, compatibility converter path, C-Blosc path, and runtime existence checks.

## Supported data

The package supports HouIO's polygon, embedded `PackedGeometry`, named `PackedFragment`, external `PackedDisk`, `PackedDiskSequence`, numeric/string/dictionary attribute, group, and dense scalar-volume model. Packed-disk authored filenames, expansion frames/policies, transforms, viewport LOD, and flags are preserved without opening or copying the referenced file. Packed-disk sequences additionally preserve their ordered sample lists, fractional index, and wrap mode. Houdini Volume Visualization detail metadata is preserved in both the scalar-attribute layout used by Houdini 20.x and the dictionary layout used by Houdini 21.x and newer.

Existing native VDB primitive payloads are preserved losslessly by file-to-file HouIO round trips. The standalone C++ API includes dependency-neutral sparse FloatGrid editing. A separately built OpenVDB-enabled library exposes standalone `.vdb` I/O through `OpenVdbBackend` and can construct Houdini-native VDB primitive payloads from `SparseFloatGrid`. The default distributed Houdini package is backend-off: its CLI preserves existing native payloads but does not expose standalone `.vdb` read/write or constructed sparse VDB output. Live HOM extraction supports exact scalar Float VDBs and rejects ambiguous activity rather than approximating it. The compatibility round-trip path can still densify supported Float SDF/Fog grids and restore their class.

File-to-file conversion preserves packed fragments, and direct **Write Selected Geometry** supports both `PackedDisk` and `PackedDiskSequence` references. Direct extraction of `hou.PackedFragment` remains unavailable because HOM does not expose its embedded source detail. Other unsupported examples include agents, height fields, ambiguous or tiled live VDB activity, and vector VDB construction/editing.

## Runtime model

The package does not load a HouIO extension or HDK plug-in into Houdini. It imports Python code, extracts supported data into the HouIO-owned `houio.hom/1` manifest, and starts `houio.exe` as a separate custom-writer process. `houio_convert.exe` remains bundled for compatibility workflows.

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
