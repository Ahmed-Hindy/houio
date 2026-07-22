# Using HouIO in Houdini 22 on Windows

This guide covers Houdini 22 on Windows 10 using the tested HOM bridge and the standalone `houio_convert.exe` process. The bridge does not load a CPython extension into Houdini, so it is compatible with Houdini 22's Python 3.13 runtime.

Users who only need the packaged Houdini tools should follow [Installing HouIO in Houdini](houdini-package.md). The build instructions below are for developers working from a source checkout.

The commands below use Houdini `22.0.368`. Change the patch version if another Houdini 22 build is installed.

## 1. Build HouIO

Open **Developer PowerShell for Visual Studio 2022**, then run:

```powershell
$HouIORoot = "G:\Projects\Dev\Github\houio"
$Hython = "C:\Program Files\Side Effects Software\Houdini 22.0.368\bin\hython.exe"

Set-Location $HouIORoot

cmake --preset windows-msvc-release `
  -DHOUIO_HYTHON_EXECUTABLE="$Hython"
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release
```

The converter is created at:

```text
G:\Projects\Dev\Github\houio\build\windows-msvc-release\houio_convert.exe
```

The Houdini-specific tests are registered only when `HOUIO_HYTHON_EXECUTABLE` points to a valid `hython.exe`.

## 2. Install the source-checkout package

From the repository root, run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\houdini\install_hom_bridge.ps1 `
  -HoudiniVersions 22.0 `
  -RepositoryRoot "G:\Projects\Dev\Github\houio" `
  -ConverterExecutable "G:\Projects\Dev\Github\houio\build\windows-msvc-release\houio_convert.exe"
```

The installer writes `houio.json` under the Windows **Documents** known folder, which may be redirected. Print the exact path with:

```powershell
$Documents = [Environment]::GetFolderPath(
  [Environment+SpecialFolder]::MyDocuments
)
Join-Path $Documents "houdini22.0\packages\houio.json"
```

The package configures:

- `PYTHONPATH` for `houio_hom`.
- `HOUIO_CONVERT_EXECUTABLE` for the C++ converter.
- `HOUIO_BLOSC_LIBRARY` as `$HFS/bin/blosc.dll` for `.bgeo.sc` support.
- `HOUIO_ROOT` and `HOUIO_PYTHON_ROOT` for discovery and diagnostics.

Restart Houdini 22 after installing or changing the package.

## 3. Verify the installation

Open **Windows > Python Source Editor** or a Python Shell pane in Houdini and run:

```python
import os
import hou
import houio_hom

print(hou.applicationVersionString())
print(houio_hom.__file__)
print(os.environ["HOUIO_CONVERT_EXECUTABLE"])
print(os.environ["HOUIO_BLOSC_LIBRARY"])
```

Expected results:

- Houdini reports a `22.0` version.
- `houio_hom` resolves inside the HouIO checkout or install prefix.
- The converter path points to an existing `houio_convert.exe`.
- The Blosc path resolves under the active Houdini installation.

## 4. Convert files from Houdini

### `.bgeo.sc` to `.bgeo.sc`

Run this from the Python Shell, Python Source Editor, or a shelf tool:

```python
from houio_hom import convert_with_houio

convert_with_houio(
    r"G:\cache\input.bgeo.sc",
    r"G:\cache\output_houio.bgeo.sc",
)
```

HouIO reads the SCF wrapper natively, processes the uncompressed Houdini geometry, and writes a new `.bgeo.sc` file.

### Float SDF or Fog VDB to `.bgeo.sc`

```python
from houio_hom import convert_with_houio

convert_with_houio(
    r"G:\cache\density.vdb",
    r"G:\cache\density_houio.bgeo.sc",
)
```

Houdini converts 32-bit Float VDB grids to dense Houdini volumes before the C++ converter runs. Level sets become iso volumes and Fog grids become smoke volumes. The `houio_vdb_class` primitive attribute preserves the authoritative `level set` or `fog volume` class through the dense representation. Polygon primitives in the same geometry are preserved during this conversion.

### Dense volume back to VDB

```python
from houio_hom import convert_with_houio

convert_with_houio(
    r"G:\cache\density.bgeo.sc",
    r"G:\cache\density_houio.vdb",
)
```

VDB output requires geometry containing only dense volumes or VDB grids. Iso dense volumes become level-set VDBs and smoke dense volumes become Fog VDBs; explicit `houio_vdb_class` metadata overrides visualization-based inference. The bridge rejects mixed mesh/volume output because Houdini's `.vdb` writer would otherwise omit mesh primitives.

## 5. Process SOP geometry

### Round-trip another SOP node

```python
import hou
from houio_hom import roundtrip_node_geometry

source_node = hou.node("/obj/geo1/OUT")
processed_geometry = roundtrip_node_geometry(source_node)

print(len(processed_geometry.points()))
print(len(processed_geometry.prims()))
```

`roundtrip_node_geometry()` returns a new `hou.Geometry`. It does not modify the source node.

Float SDF and Fog VDB primitives are explicitly converted to iso or smoke dense volumes before HouIO processes the geometry. Their semantic class is preserved for later VDB reconstruction.

### Use inside a Python SOP

Place this in a Python SOP:

```python
from houio_hom import roundtrip_current_sop

roundtrip_current_sop()
```

The Python SOP's writable geometry is replaced with the HouIO round-trip result.

This operation is synchronous. The converter timeout defaults to 300 seconds and can be changed per call:

```python
from houio_hom import roundtrip_current_sop

roundtrip_current_sop(timeout_seconds=900.0)
```

Use `timeout_seconds=None` only when an unbounded subprocess is intentional.

## 6. Use from `hython`

```powershell
$Hython = "C:\Program Files\Side Effects Software\Houdini 22.0.368\bin\hython.exe"

& $Hython -m houio_hom inspect "G:\cache\input.bgeo.sc"
& $Hython -m houio_hom decode "G:\cache\density.vdb" "G:\cache\density.bgeo"
& $Hython -m houio_hom encode "G:\cache\density.bgeo" "G:\cache\density.bgeo.sc"
& $Hython -m houio_hom encode "G:\cache\density.bgeo" "G:\cache\density.vdb"
```

This is the recommended interface for headless pipeline jobs because it uses Houdini's geometry readers, writers, and Convert VDB SOP implementation without opening the desktop UI.

## 7. Use the converter directly outside Houdini

```powershell
$Converter = "G:\Projects\Dev\Github\houio\build\windows-msvc-release\houio_convert.exe"
$env:HOUIO_BLOSC_LIBRARY = "C:\Program Files\Side Effects Software\Houdini 22.0.368\bin\blosc.dll"

& $Converter "G:\cache\input.bgeo.sc" "G:\cache\output.bgeo"
& $Converter "G:\cache\output.bgeo" "G:\cache\output.bgeo.sc"
```

The standalone converter supports `.geo`, `.bgeo`, and `.bgeo.sc`. Native sparse `.vdb` parsing is not implemented in the C++ library; use the HOM bridge for VDB files.

## 8. Troubleshooting

### `ModuleNotFoundError: No module named 'houio_hom'`

Check the package path used by Windows:

```powershell
$Documents = [Environment]::GetFolderPath(
  [Environment+SpecialFolder]::MyDocuments
)
$PackagePath = Join-Path $Documents "houdini22.0\packages\houio.json"
Test-Path $PackagePath
$PackagePath
```

Then restart Houdini. In the Python Shell, inspect:

```python
import os
print(os.environ.get("HOUIO_PYTHON_ROOT"))
print(os.environ.get("PYTHONPATH"))
```

### `Could not find houio_convert`

Re-run the installer with an explicit executable:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\houdini\install_hom_bridge.ps1 `
  -HoudiniVersions 22.0 `
  -RepositoryRoot "G:\Projects\Dev\Github\houio" `
  -ConverterExecutable "G:\Projects\Dev\Github\houio\build\windows-msvc-release\houio_convert.exe"
```

### `.bgeo.sc` reports that C-Blosc is unavailable

Verify:

```python
import os
from pathlib import Path

blosc_path = Path(os.environ["HOUIO_BLOSC_LIBRARY"])
print(blosc_path)
print(blosc_path.is_file())
```

For Houdini 22, `blosc.dll` may depend on adjacent DLLs such as `zstd.dll`. Keep the Houdini `bin` directory intact; HouIO resolves adjacent dependencies while loading the selected Blosc runtime.

### VDB conversion consumes too much memory

The bridge converts sparse VDB grids to dense Houdini volumes. Restrict this workflow to bounded grids whose dense voxel allocation is acceptable. Native sparse OpenVDB topology is not preserved.

### Unsupported primitive errors

HouIO's strongest standalone paths are polygons and legacy dense scalar volumes. Packed geometry, agents, native sparse VDB primitives, height fields, and many other Houdini primitive types are not supported by the C++ model.

## 9. Uninstall

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\houdini\install_hom_bridge.ps1 `
  -HoudiniVersions 22.0 `
  -Uninstall
```

Restart Houdini after uninstalling the package.
