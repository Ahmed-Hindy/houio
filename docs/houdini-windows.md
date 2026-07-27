# Houdini integration on Windows

This guide covers development and validation with Houdini 20.0 or newer.

HouIO integrates through HOM and an external `houio_convert.exe` process. It does not load a CPython extension or HDK plug-in into Houdini.

## Supported versions

The package validation suite currently passes in:

- Houdini 20.0.653
- Houdini 20.5.410
- Houdini 21.0.631
- Houdini 22.0.368

The minimum supported version is Houdini 20.0.

## Build

From the project root:

```powershell
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release
```

The converter is written to:

```text
build/windows-msvc-release/houio_convert.exe
```

## Development install from the source tree

Install the HOM bridge for all supported major/minor versions:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\houdini\install_hom_bridge.ps1 `
  -RepositoryRoot "$PWD" `
  -ConverterExecutable "$PWD\build\windows-msvc-release\houio_convert.exe"
```

Install one version only:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\houdini\install_hom_bridge.ps1 `
  -RepositoryRoot "$PWD" `
  -ConverterExecutable "$PWD\build\windows-msvc-release\houio_convert.exe" `
  -HoudiniVersions 20.0
```

Restart Houdini after changing package files.

The installer writes package definitions under the Windows Documents folder:

```text
houdini20.0/packages/houio.json
houdini20.5/packages/houio.json
houdini21.0/packages/houio.json
houdini22.0/packages/houio.json
```

## Verify the environment

Open Houdini and run this in the Python shell:

```python
import os
import hou
import houio_hom

print(hou.applicationVersionString())
print(houio_hom.__file__)
print(os.environ.get("HOUIO_CONVERT_EXECUTABLE"))
print(hou.text.expandString(os.environ.get("HOUIO_BLOSC_LIBRARY", "")))
```

Confirm:

- Houdini reports version 20.0 or newer.
- `houio_hom` resolves from the intended source or install tree.
- `HOUIO_CONVERT_EXECUTABLE` points to the Release executable.
- `HOUIO_BLOSC_LIBRARY` resolves to `$HFS/bin/blosc.dll`.

## Test the converter outside Houdini

```powershell
.\build\windows-msvc-release\houio_convert.exe `
  input.bgeo `
  output.bgeo.sc
```

Load the output with a File SOP and verify geometry counts and attributes.

## Python API

Convert a file:

```python
from houio_hom import convert_with_houio

convert_with_houio(
    "input.bgeo.sc",
    "output.bgeo.sc",
    timeout_seconds=300.0,
)
```

Load geometry for HouIO-compatible processing:

```python
from houio_hom import load_for_houio

geometry = load_for_houio("input.bgeo.sc", convert_vdb=False)
print(len(geometry.points()))
print(len(geometry.prims()))
```

Round-trip the current Python SOP:

```python
from houio_hom import roundtrip_current_sop

roundtrip_current_sop(timeout_seconds=300.0)
```

## Package tools

Inside a SOP network, press **Tab** and open the **HouIO** submenu.

### HouIO Round Trip

Creates a Python SOP after the selected node. Verify:

- The node connects to the selected source.
- It cooks without errors or warnings.
- Point and primitive counts match.
- **Enabled** bypasses processing when disabled.
- **Timeout (seconds)** is present and finite.

### Convert Geometry File

Runs the external converter for a selected input and output path.

### Package Diagnostics

Reports:

- Houdini version
- Package root
- Primary writer path and `houio diagnose --json` result
- Compatibility converter path
- C-Blosc path
- Existence checks for each runtime dependency

## VDB workflow

HouIO preserves existing native Houdini VDB primitive payloads losslessly during file-to-file GEO/BGEO/SCF round trips. The default build remains independent of OpenVDB, but `SparseFloatGrid` supports dependency-neutral sparse editing and `HOUIO_ENABLE_OPENVDB=ON` adds native `.vdb` FloatGrid I/O plus Houdini-native VDB primitive construction. Live HOM extraction is available for exact scalar Float VDBs and rejects grids whose activity cannot be determined without approximation.

The compatibility bridge also supports bounded 32-bit Float grids by converting them explicitly:

- SDF grids become dense iso volumes.
- Fog grids become dense smoke volumes.
- `houio_vdb_class` preserves the source class.
- Output conversion restores the VDB class.

Example:

```python
from houio_hom import convert_with_houio

convert_with_houio(
    "density.vdb",
    "density_houio.bgeo.sc",
    timeout_seconds=300.0,
)
```

Densification may use substantially more memory than the sparse source. Bound resolution before running this workflow.

## Headless package validation

Build the package archive:

```powershell
cmake --build --preset windows-msvc-release --target houio_houdini_package
```

Run the package test with a selected Houdini installation:

```powershell
$CMake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$Archive = "$PWD\build\windows-msvc-release\houio-houdini-package-0.2.0-windows-x86_64.zip"
$Hython = "C:\Program Files\Side Effects Software\Houdini 20.0.653\bin\hython.exe"

& $CMake `
  "-DHOUIO_HOUDINI_PACKAGE_ARCHIVE=$Archive" `
  "-DHOUIO_HOUDINI_PACKAGE_EXTRACT_DIR=$PWD\build\package-test-20.0.653" `
  "-DHOUIO_HYTHON_EXECUTABLE=$Hython" `
  "-DHOUIO_HOUDINI_PACKAGE_TEST_SCRIPT=$PWD\tools\houdini\test_houdini_package.py" `
  -P "$PWD\tests\run_houdini_package_test.cmake"
```

The test validates environment paths, Python imports, shelf tools, diagnostics, and a box round trip.

## Fixture matrix

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\houdini\run_fixture_roundtrips.ps1
```

The matrix generates files in the build tree, round-trips them through HouIO, and compares exact geometry state in Houdini.

## Uninstall development package files

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\houdini\install_hom_bridge.ps1 `
  -HoudiniVersions 20.0,20.5,21.0,22.0 `
  -Uninstall
```

Restart Houdini after uninstalling.

## Troubleshooting

### Module cannot be imported

Check the package JSON and `PYTHONPATH`, then restart Houdini.

### Writer or converter is missing

Rebuild the Release preset. Verify `HOUIO_EXECUTABLE` for the primary custom writer and `HOUIO_CONVERT_EXECUTABLE` for compatibility workflows.

### `.bgeo.sc` fails

Verify that `HOUIO_BLOSC_LIBRARY` expands to the active Houdini installation's `bin\blosc.dll`. Keep adjacent dependency DLLs in the same Houdini `bin` directory.

### VDB conversion uses too much memory

Use file-to-file native VDB pass-through when possible. Exact scalar Float VDBs can use the sparse live-HOM manifest path with an OpenVDB-enabled HouIO executable. The compatibility live-HOM conversion path remains appropriate when exact sparse activity is unavailable; reduce its resolution when using that dense path.

### Round Trip node times out

Increase the node timeout only after checking file size, primitive support, and converter diagnostics.
