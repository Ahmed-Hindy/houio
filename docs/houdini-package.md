# Installing the HouIO Houdini package

The Windows package contains:

- `houio_convert.exe`
- `houio_hom` Python bridge
- Shelf and Tab-menu tools
- Package diagnostics
- PowerShell installer

The minimum supported Houdini version is 20.0.

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

## Install with PowerShell

Extract the ZIP and run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\install_houdini_package.ps1
```

The installer:

- Copies package files to `%LOCALAPPDATA%\HouIO`.
- Creates one loader per selected Houdini version in the Windows Documents folder.
- Targets 20.0, 20.5, 21.0, and 22.0 by default.

Install one version:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\install_houdini_package.ps1 `
  -HoudiniVersions 20.0
```

Restart Houdini after installation.

## Houdini 22 Package Browser

Houdini 22 can install the ZIP directly:

1. Open **Inspectors > Package Browser**.
2. Choose **File > Install Package Archive**.
3. Select the HouIO ZIP.
4. Accept the installation location.

The tools should become available immediately.

## Tools

Open a SOP network and press **Tab > HouIO**.

### HouIO Round Trip

Creates a configured Python SOP after the selected SOP.

Parameters:

- **Enabled** — bypasses HouIO when disabled.
- **Timeout (seconds)** — limits converter execution; default `300`.

### Convert Geometry File

Converts `.geo`, `.bgeo`, or `.bgeo.sc` through the bundled executable.

### Package Diagnostics

Reports the active package root, Houdini version, converter path, C-Blosc path, and runtime existence checks.

## Manual acceptance test

1. Create a Geometry object and enter its SOP network.
2. Create and select a Box SOP.
3. Run **Tab > HouIO > Package Diagnostics**.
4. Confirm package, converter, and Blosc checks pass.
5. Run **Tab > HouIO > HouIO Round Trip**.
6. Confirm the created node cooks without errors or warnings.
7. Compare point and primitive counts with the source Box.
8. Disable **Enabled** and confirm geometry passes through unchanged.
9. Use **Convert Geometry File** to write `.bgeo.sc`.
10. Load the output with a File SOP.

Repeat the diagnostics and Box round trip in every Houdini version you intend to support.

## Supported data

The package supports HouIO's polygon, attribute, group, and dense scalar-volume model.

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

This removes the selected package loaders and installed HouIO files.

Keep installed files while removing loaders:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  "$env:LOCALAPPDATA\HouIO\install_houdini_package.ps1" `
  -Uninstall `
  -KeepFiles
```

## Development setup

For a package that points directly at a source checkout and local converter build, use `tools/houdini/install_hom_bridge.ps1` and follow [Houdini integration on Windows](houdini-windows.md).
