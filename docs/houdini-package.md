# Installing HouIO in Houdini

HouIO is distributed as a small Windows ZIP containing the converter, Python bridge, shelf tools, and a Houdini package file.

## Houdini 22 Package Browser

1. Download `houio-houdini-package-<version>-windows-x86_64.zip`.
2. In Houdini, open a new pane and choose **Inspectors > Package Browser**.
3. Choose **File > Install Package Archive**.
4. Select the downloaded ZIP and accept the installation location.

The tools become available immediately. A restart is not required.

## PowerShell installation for Houdini 21 or 22

Extract the ZIP, open PowerShell in the extracted folder, and run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\install_houdini_package.ps1
```

The installer copies HouIO to `%LOCALAPPDATA%\HouIO` and writes small package-loader files for Houdini 21.0 and 22.0. Restart Houdini after using this installation method.

Install only one Houdini version:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\install_houdini_package.ps1 `
  -HoudiniVersions 22.0
```

The installer is retained under `%LOCALAPPDATA%\HouIO`, so the extracted download can be deleted after installation.

Uninstall:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  "$env:LOCALAPPDATA\HouIO\install_houdini_package.ps1" `
  -Uninstall
```

## Using the package

Open a SOP network and press **Tab**. The **HouIO** submenu contains:

- **HouIO Round Trip**: creates a configured Python SOP after the selected SOP.
- **Convert Geometry File**: converts a geometry file through the bundled converter.
- **Package Diagnostics**: reports the active package, converter, and Blosc paths.

The Round Trip node contains:

- **Enabled**: bypasses HouIO processing when disabled.
- **Timeout (seconds)**: limits the external converter runtime. The default is 300 seconds.

The node runs the converter in a separate process. Houdini does not load a HouIO CPython extension or HDK plug-in.

## Supported Houdini versions and licenses

The same archive has been tested with:

- Houdini 21.0.631
- Houdini 22.0.368
- Houdini FX
- Houdini Core
- Houdini Apprentice

The package contains no licensed SideFX binaries. It resolves the active Houdini installation's `$HFS/bin/blosc.dll` at runtime for `.bgeo.sc` support.

## Data limitations

The package supports HouIO's polygon and dense scalar-volume model. Houdini-side VDB conversion accepts bounded 32-bit Float level-set and Fog grids. Sparse VDB topology is converted to dense Houdini volumes, while level-set or Fog semantics are retained.

Packed primitives, agents, height fields, vector VDB grids, and many other Houdini primitive types remain unsupported.

## Building the archive

From a configured Windows Release build:

```powershell
cmake --build --preset windows-msvc-release `
  --target houio_houdini_package
```

The archive is written to:

```text
build/windows-msvc-release/houio-houdini-package-<version>-windows-x86_64.zip
```
