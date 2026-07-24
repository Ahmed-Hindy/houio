# HouIO

HouIO is a C++20 library and Windows Houdini package for reading, writing, inspecting, and round-tripping Houdini geometry files without linking against the Houdini Development Kit.

Supported containers:

- `.geo`
- `.bgeo`
- `.bgeo.sc`
- `.vdb` through the Houdini Python bridge

The minimum supported Houdini version is **20.0**. The package is validated with Houdini 20.0.653, 20.5.410, 21.0.631, and 22.0.368.

> [!IMPORTANT]
> This project does not currently include a project-wide license file. Do not redistribute source or binaries until licensing and third-party provenance are resolved.

## Components

| Component | Purpose |
| --- | --- |
| `houio::houio` | Static C++ library for GEO, BGEO, and SCF geometry I/O |
| `houio_convert` | Command-line file converter |
| `python/houio_hom` | Houdini Python bridge for package and VDB workflows |
| Houdini package | Shelf tools, Python SOP round trip, diagnostics, and converter access |
| Test suite | Parser, schema, topology, attribute, volume, package, sanitizer, and fuzz coverage |

## Supported geometry

HouIO currently supports:

- Point, vertex, primitive, and global attribute domains
- Signed Int32 and Int64 storage
- Float16, Float32, and Float64 storage
- Indexed string attributes, including Houdini's empty-string sentinel
- Indexed dictionary metadata in faithful `HouGeo` round trips
- Unordered point, vertex, and primitive groups
- `Poly`, `Polygon_run`, and `PolygonCurve_run`
- Dense scalar volumes
- SCF compression through C-Blosc

The simplified `Geometry` API is intentionally render-oriented and may split points at vertex-attribute discontinuities. Use `HouGeo` and `HouGeoAdapter` when domain fidelity matters.

Not currently supported by the standalone C++ model:

- Packed primitives
- Agents and crowds
- Height fields
- Native sparse OpenVDB trees
- Vector VDB grids
- NURBS and Bezier primitives
- Instancing records

The Houdini bridge can explicitly convert supported Float SDF and Fog VDB grids to dense volumes, process them through HouIO, and restore their VDB class on output. It also preserves Houdini Volume Visualization detail metadata across the supported Houdini versions.

## Build

Requirements:

- CMake 3.24 or newer
- A C++20 compiler
- Visual Studio 2022 on Windows, or GCC/Clang on Linux

Windows Debug:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

Windows Release:

```powershell
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release
```

Linux GCC:

```bash
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release
ctest --preset linux-gcc-release
```

## C++ API

### Read a Houdini-oriented geometry object

```cpp
#include <houio/GeometryIO.h>

const auto result = houio::GeometryIO::readHouGeo("asset.bgeo.sc");
if (!result)
{
    for (const houio::Diagnostic& diagnostic : result.diagnostics)
    {
        // Report diagnostic.message, diagnostic.path, and diagnostic.byteOffset.
    }
    return;
}

houio::HouGeo::Ptr geometry = result.value;
```

### Read the simplified mesh representation

```cpp
const auto result = houio::GeometryIO::readGeometry("mesh.bgeo");
if (!result)
    return;

houio::Geometry::Ptr mesh = result.value;
```

### Read all dense scalar volumes

```cpp
const auto result = houio::GeometryIO::readVolumes("volumes.bgeo.sc");
if (!result)
    return;

for (const houio::ScalarField::Ptr& volume : result.value)
{
    const houio::math::V3i resolution = volume->resolution();
}
```

### Write geometry

```cpp
houio::GeometryWriteOptions options;
options.format = houio::GeometryFileFormat::bgeo_scf;

const auto result = houio::GeometryIO::writeGeometry(
    "mesh.bgeo.sc",
    mesh,
    options
);
```

### Configure parser limits

```cpp
houio::GeometryReadOptions options;
options.maxFileBytes = 256ULL * 1024ULL * 1024ULL;
options.parserLimits.maxStringBytes = 16ULL * 1024ULL * 1024ULL;
options.parserLimits.maxUniformArrayElements = 64ULL * 1024ULL * 1024ULL;
options.parserLimits.maxNestingDepth = 256;

const auto result = houio::GeometryIO::readHouGeo("asset.bgeo", options);
```

## Command-line converter

```powershell
houio_convert input.bgeo output.bgeo.sc
houio_convert input.bgeo.sc output.bgeo
```

Use `HOUIO_BLOSC_LIBRARY` or `GeometryWriteOptions::bloscLibraryPath` when C-Blosc cannot be resolved automatically.

## Houdini package

Build the package archive:

```powershell
cmake --build --preset windows-msvc-release --target houio_houdini_package
```

Generated archive:

```text
build/windows-msvc-release/houio-houdini-package-<version>-windows-x86_64.zip
```

Test an extracted archive without installing anything:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\bootstrap_houdini_package.ps1 `
  -HoudiniVersion 20.0
```

The bootstrap script launches Houdini with isolated process-local package and preference directories. It does not copy files into AppData or write to any Houdini user package folder. Close Houdini to remove the temporary bootstrap directory.

A persistent installation requires the explicit `-Install` action:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\install_houdini_package.ps1 `
  -Install
```

The persistent installer targets Houdini 20.0, 20.5, 21.0, and 22.0 by default.

Inside a SOP network, use:

- **HouIO Round Trip**
- **Convert Geometry File**
- **Package Diagnostics**

See [Installing HouIO in Houdini](docs/houdini-package.md) for the package workflow and [Houdini integration on Windows](docs/houdini-windows.md) for development setup.

## Compatibility validation

The generated fixture matrix covers:

- Empty and point-only geometry
- Numeric and string attributes across all supported domains
- Multi-page attributes and constant-page compression
- Triangles, quads, mixed polygon sizes, and n-gons
- Open polygon curves
- UV seams
- Multiple primitive records
- Dense scalar volumes
- Point, vertex, and primitive groups

Run it with installed Houdini versions:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\houdini\run_fixture_roundtrips.ps1
```

A static Crag round-trip is also available:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\houdini\run_crag_roundtrip.ps1
```

## Development checks

AddressSanitizer:

```powershell
cmake --preset windows-msvc-asan
cmake --build --preset windows-msvc-asan
ctest --preset windows-msvc-asan
```

UndefinedBehaviorSanitizer:

```bash
cmake --preset linux-gcc-ubsan
cmake --build --preset linux-gcc-ubsan
ctest --preset linux-gcc-ubsan
```

Optional libFuzzer target:

```bash
cmake --preset linux-clang-fuzzer
cmake --build --preset linux-clang-fuzzer
./build/linux-clang-fuzzer/houio_fuzz_parser -runs=2000 -max_len=512 -timeout=5
```

## Documentation

- [Architecture](architecture.md)
- [Developer onboarding](onboard.md)
- [Houdini package](docs/houdini-package.md)
- [Houdini integration on Windows](docs/houdini-windows.md)
- [Security policy](SECURITY.md)
- [Roadmap](todo.md)
