# HouIO

HouIO is a C++20 library and Windows Houdini package for reading, writing, inspecting, and round-tripping Houdini geometry files without linking against the Houdini Development Kit.

Supported containers:

- `.geo`
- `.bgeo`
- `.bgeo.sc`
- `.vdb` through the Houdini Python bridge

The minimum supported Houdini version is **20.0**. The file, large-asset, and package matrices are validated with Houdini 20.0.653, 20.5.410, 21.0.631, and 22.0.368. See the [compatibility matrix](docs/compatibility.md).

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
- Attribute definition scope and complete semantic `options` objects
- Unordered point, vertex, and primitive groups, including groups spanning mixed polygon and volume records
- `Poly`, `Polygon_run`, and `PolygonCurve_run`
- Dense scalar volumes
- SCF compression through C-Blosc

The simplified `Geometry` API is intentionally render-oriented and may split points at vertex-attribute discontinuities. It supports fixed-size point, line, triangle, and quad sets plus one arbitrary n-gon; multiple variable-size polygons remain a Houdini-oriented-model concern. Use `HouGeo` and `HouGeoAdapter` when domain fidelity matters.

`<houio/GeometryModels.h>` provides intention-revealing, non-breaking aliases: `HoudiniGeometry` for the supported Houdini-oriented model and `SimplifiedMesh` for the render-oriented convenience model.

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

For explicit split and loss reporting, convert one Houdini-oriented primitive directly:

```cpp
const auto source = houio::GeometryIO::readHouGeo("mesh.bgeo");
if (!source || source.value->primitives().empty())
    return;

const houio::GeometryConversionResult conversion =
    houio::HouGeoIO::convertToGeometryResult(
        source.value,
        source.value->primitives().front());
if (!conversion)
{
    // Inspect conversion.diagnostics.
    return;
}

houio::Geometry::Ptr mesh = conversion.value;
const std::size_t duplicated_points = conversion.report.duplicatedPointCount;
```

The report also lists skipped point, vertex, primitive, and global attributes; dropped groups; source/output point counts; split source points; and winding reversal.

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

### Experimental native field persistence

```cpp
#include <houio/FieldIO.h>

const bool stored = houio::storeField(*volume, "volume.field");
const houio::ScalarField::Ptr loaded = houio::loadField<float>("volume.field");
```

This installed API is opt-in, but the current native binary layout is experimental, platform-dependent, and not covered by stable interchange guarantees. See [Experimental field persistence format](docs/field-format.md).

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

Run it with the maintained Houdini 20.0.653, 20.5.410, 21.0.631, and 22.0.368 matrix:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\houdini\run_fixture_roundtrips.ps1
```

The same four-version matrix validates each output inside Houdini. A large Crag round-trip is also available:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\houdini\run_crag_roundtrip.ps1
```

Validate the generated Houdini package across the maintained matrix:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\houdini\run_package_matrix.ps1
```

See [Fixture generation and validation](docs/fixtures.md) for the manifest, known-loss, and extension workflow.

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

Opt-in performance baselines:

```powershell
cmake --preset windows-msvc-benchmarks
cmake --build --preset windows-msvc-benchmarks
.\build\windows-msvc-benchmarks\houio_benchmarks.exe
```

See [Performance baselines](docs/benchmarks.md) for methodology and workload controls.

## Documentation

- [Architecture](architecture.md)
- [Developer onboarding](onboard.md)
- [Contributing](CONTRIBUTING.md)
- [Compatibility matrix](docs/compatibility.md)
- [Fixture generation and validation](docs/fixtures.md)
- [Performance baselines](docs/benchmarks.md)
- [Experimental field persistence format](docs/field-format.md)
- [Versioning and release policy](docs/versioning.md)
- [Houdini package](docs/houdini-package.md)
- [Houdini integration on Windows](docs/houdini-windows.md)
- [Security policy](SECURITY.md)
- [Roadmap](todo.md)
