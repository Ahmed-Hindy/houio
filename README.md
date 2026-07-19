# HouIO

HouIO is a small C++20 library for reading and writing Houdini geometry files without linking against Houdini or the Houdini Development Kit.

It implements SideFX's JSON-based geometry encoding and the Seekable Compressed Format wrapper used by:

- `.geo` ASCII geometry files
- `.bgeo` binary geometry files
- `.bgeo.sc` Blosc-compressed geometry files

HouIO is a standalone file-format library. It is **not** a Houdini plug-in, does not load `.hip` or `.hipnc` scene files, and does not require a Houdini installation at runtime.

> [!IMPORTANT]
> The repository currently has no license file. Public source availability does not grant permission to redistribute or embed the code. Resolve licensing with the original author before using HouIO in a distributed product.

## Project status

HouIO should currently be treated as an experimental legacy library rather than a production-ready dependency.

- The included geometry fixtures identify themselves as Houdini `13.0.288` files.
- The latest upstream commit is from 2020.
- A static Crag round-trip and a 14-case minimal geometry matrix have been validated with Houdini `21.0.631` and `22.0.368`.
- Polygon, `.bgeo.sc`, and legacy dense-volume paths are the best-developed areas.
- Packed geometry, native OpenVDB primitives, curves, agents, height fields, and most modern primitive types are not supported by the standalone C++ model.
- The optional HOM bridge converts scalar `.vdb` files to and from HouIO's dense-volume representation inside Houdini.

See [todo.md](todo.md) for the modernization plan.

## Main capabilities

- Parse Houdini ASCII and binary JSON.
- Inspect unknown Houdini geometry structures using a JSON logger.
- Read point, vertex, primitive, and global attribute domains.
- Preserve unordered point, vertex, and primitive groups.
- Read legacy polygon runs, Houdini 21/22 `Polygon_run` records, and open `PolygonCurve_run` records.
- Read legacy dense Houdini volumes, including tiled and constant storage.
- Read and write point, polygon, and dense-volume `.bgeo` and `.bgeo.sc` files.
- Detect formats from both file extensions and stream signatures.
- Return owned result objects with structured diagnostics and explicit success state.
- Convert supported Houdini geometry into a lightweight render-oriented mesh representation.
- Read all dense scalar volumes or use a first-volume convenience API with loss warnings.
- Export custom geometry implementations through the `HouGeoAdapter` interface.
- Use a Houdini 21/22 HOM package for `.vdb`, `.bgeo.sc`, Python SOP, shelf-tool, and headless hython workflows.

## Repository layout

```text
include/houio/       Public C++ headers
include/ttl/         Vendored variant and template utilities
src/                 Library implementation
src/math/            Math implementation
tests/               Unit, packaging, and Houdini integration tests
tools/               Installed converter and Houdini validation utilities
python/houio_hom/    Houdini Object Model bridge package
scene_exporter/      Separate legacy Houdini Python 2 scene exporter
cmake/               CMake package configuration
```

For a module-level description, see [architecture.md](architecture.md).

For a practical introduction to the codebase, see [onboard.md](onboard.md).

## Quick API examples

### Read geometry with owned diagnostics

```cpp
#include <houio/GeometryIO.h>

const houio::GeometryReadResult<houio::HouGeo::Ptr> result =
    houio::GeometryIO::readHouGeo("mesh.bgeo.sc");

if (!result)
{
    for (const houio::Diagnostic& diagnostic : result.diagnostics)
    {
        // Log category, message, byteOffset, and semantic path.
    }
}
```

`GeometryIO` is the preferred path-based API. Result objects own their diagnostics, expose explicit success state, and do not require output pointers. The returned `std::shared_ptr` owns the parsed geometry independently of the input file and temporary decompression buffers. `GeometryReadOptions::maxFileBytes` defaults to 512 MiB for the on-disk container, while `parserLimits.maxInputBytes` independently bounds the raw or decompressed document.

Use `readGeometry()` for the deliberately lossy render-oriented model. It requires point `P`, one fixed polygon size, valid point references, and domain-consistent attributes. Use `readHouGeo()` when mixed polygon sizes, primitive/global attributes, groups, multiple primitive records, or exact supported Houdini storage must survive.

### Read dense volumes

```cpp
const auto volumes = houio::GeometryIO::readVolumes("fields.bgeo.sc");
if (volumes)
{
    for (const houio::ScalarField::Ptr& field : volumes.value)
    {
        // Every returned field is independently owned by the parsed HouGeo.
    }
}
```

`readVolume()` is a convenience wrapper that returns the first dense scalar volume. It emits a conversion warning when the file contains additional volumes.

### Write geometry

```cpp
#include <houio/GeometryIO.h>

houio::Geometry::Ptr geometry = houio::Geometry::createTriangleGeometry();
// Populate P, other attributes, and indices.

const houio::GeometryWriteResult result =
    houio::GeometryIO::writeGeometry("output.bgeo.sc", geometry);
```

The output extension selects raw `.bgeo` or compressed `.bgeo.sc`. `GeometryWriteOptions` controls SCF block size, compression level, shuffling, compressor name, and an optional explicit C-Blosc library path.

`HouGeoIO::importGeometry()`, `importVolume()`, `exportGeometry()`, `exportVolume()`, and the historical `xport()` overloads remain source-compatible wrappers. The no-diagnostics import overloads throw `DiagnosticException` on failure; overloads receiving a `DiagnosticList` append errors and return null. Stream-based serialization still uses an explicit stack-owned export context and supports independent concurrent streams. ASCII geometry export remains unsupported.

### Inspect a file's structure

```cpp
#include <iostream>
#include <houio/HouGeoIO.h>

houio::HouGeoIO::makeLog("mesh.bgeo", &std::cout);
```

The logger is useful because Houdini geometry uses flattened key/value arrays and has changed across Houdini versions.

### Configure parser safety limits

Binary parsing uses conservative defaults for total input bytes, individual strings, uniform-array element counts, and nesting depth. Applications that accept untrusted or unusually large files can override them:

```cpp
houio::json::ParserLimits limits;
limits.maxInputBytes = 256LL * 1024LL * 1024LL;
limits.maxStringBytes = 8 * 1024 * 1024;
limits.maxUniformArrayElements = 16 * 1024 * 1024;
limits.maxNestingDepth = 256;

std::ifstream input("mesh.bgeo", std::ios::binary);
houio::HouGeo::Ptr geometry = houio::HouGeoIO::import(&input, limits);
```

Seekable inputs are size-checked before parsing, streaming inputs are bounded as bytes are consumed, and the parser validates the complete document rather than ignoring trailing data. Fixed-size reads reject truncated streams, uniform-array payload sizes are checked before handler allocation, encoded lengths cannot be negative, allocation byte counts are overflow-checked, duplicate map keys are rejected, and undefined string-token references fail explicitly. The defaults allow the tested Crag geometry without adjustment.

### Capture structured diagnostics

Diagnostics-aware overloads return a null pointer on failure and append caller-owned records instead of printing errors:

```cpp
houio::DiagnosticList diagnostics;
std::ifstream input("mesh.bgeo", std::ios::binary);
houio::HouGeo::Ptr geometry = houio::HouGeoIO::import(&input, &diagnostics);

for (const houio::Diagnostic& diagnostic : diagnostics)
{
    // diagnostic.severity
    // diagnostic.category
    // diagnostic.message
    // diagnostic.byteOffset: -1 when the failure is not tied to stream bytes
    // diagnostic.path: for example attributes.pointattributes[2]
}
```

`DiagnosticCategory::malformed_input` identifies invalid stream encoding, while `unsupported_input` identifies valid records HouIO does not implement. Semantic failures use `schema`, file access uses `io`, and lossy convenience conversion warnings use `conversion`.

The original overloads remain available. Parser and path-import failures throw `DiagnosticException` when no diagnostic list is supplied, while diagnostics-aware overloads capture those failures and return null. `importGeometry()`, `importVolume()`, and `convertToGeometry()` also accept diagnostic lists. `importVolume()` now reports an empty primitive list instead of indexing it.

## `.bgeo.sc` support

HouIO reads and writes SideFX's Seekable Compressed Format wrapper directly. The implementation validates the `scf1` header, metadata length, Blosc block headers, seek index, nominal and final block sizes, decompressed-size limit, and `1fcs` footer before passing the uncompressed bgeo payload to the existing parser.

HouIO dynamically loads C-Blosc so the core library has no required compression link dependency. Resolution order is:

1. `GeometryReadOptions::bloscLibraryPath` or `GeometryWriteOptions::bloscLibraryPath`.
2. `HOUIO_BLOSC_LIBRARY`.
3. `$HFS/bin/blosc.dll` on Windows.
4. Standard platform library names such as `blosc.dll` or `libblosc.so.1`.

Inside Houdini, the supplied package sets `HOUIO_BLOSC_LIBRARY` to the Blosc runtime shipped with the active Houdini version. Outside Houdini, install C-Blosc or pass its shared-library path explicitly.

The installed converter can transcode directly:

```powershell
houio_convert input.bgeo.sc output.bgeo
houio_convert input.bgeo output.bgeo.sc
```

Both directions are tested against Houdini-generated files, and Houdini 21 and 22 load HouIO-generated SCF output with exact tested topology and attributes.

## Houdini HOM and `.vdb` workflows

The standalone C++ library does not link to Houdini's private OpenVDB build and does not yet model sparse OpenVDB trees natively. The `python/houio_hom` package provides the supported Houdini-side path:

- Houdini reads and writes `.bgeo`, `.bgeo.sc`, `.geo`, and `.vdb` through HOM.
- 32-bit float VDB grids can be converted to dense Houdini volumes with the `convertvdb` SOP verb before HouIO processing.
- Dense HouIO volumes can be converted back to VDB grids before `.vdb` output.
- `hou.Geometry.data()` and `hou.Geometry.load()` bridge uncompressed bgeo bytes without temporary Houdini nodes.
- `houio_convert` can be launched from HOM without loading a Python extension into Houdini's process.

Install the package for the current checkout:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\houdini\install_hom_bridge.ps1
```

This writes `houio.json` under the user package directories for Houdini 21.0 and 22.0. It supports either a source checkout or an installed HouIO prefix, sets `HOUIO_ROOT` and `HOUIO_PYTHON_ROOT`, prepends the HOM package to `PYTHONPATH`, points `HOUIO_BLOSC_LIBRARY` at `$HFS/bin/blosc.dll`, and records `HOUIO_CONVERT_EXECUTABLE` when the converter is available. Use `-Uninstall` to remove those package files.

### Python shell or shelf tool

```python
from houio_hom import convert_with_houio, load_for_houio

# VDB is converted to a dense volume, processed by HouIO, then written as SCF.
convert_with_houio("density.vdb", "density_houio.bgeo.sc")

geometry = load_for_houio("density_houio.bgeo.sc", convert_vdb=False)
print(len(geometry.prims()))
```

### Python SOP

```python
from houio_hom import roundtrip_current_sop

# Replaces the writable Python SOP geometry with HouIO's round-trip output.
# Float VDB primitives are explicitly densified before the subprocess call.
roundtrip_current_sop(timeout_seconds=300.0)
```

### Headless hython

```powershell
hython -m houio_hom decode density.vdb density.bgeo
hython -m houio_hom encode density.bgeo density.bgeo.sc
hython -m houio_hom encode density.bgeo density.vdb
```

VDB conversion is deliberately bounded. VDB-to-volume conversion accepts 32-bit float grids and preserves unrelated primitives in mixed Houdini geometry. Volume-to-VDB conversion requires a pure dense-volume set because Houdini's `.vdb` writer silently omits mesh primitives. Sparse grids become dense in memory, so this path is intended for bounded fields, not large production VDBs. Converter subprocess calls default to a 300-second timeout, which can be changed or disabled per call.

## Building

HouIO builds as a static C++20 library using target-based CMake.

For Windows integration, use an MSVC Developer PowerShell or Developer Command Prompt:

```powershell
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release
```

The release preset uses Ninja, builds the library, `houio_convert`, examples, and tests. Unit and package-consumer tests always run; HOM, SCF cross-compatibility, fixture, and Crag tests are registered when `HOUIO_HYTHON_EXECUTABLE` points to an installed `hython.exe`. Set `HOUIO_BUILD_TOOLS=OFF` when only the library is required.

Linux and other GCC-based environments can use the matching C++20 preset:

```bash
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release
ctest --preset linux-gcc-release
```

CI verifies both Visual Studio 2022 on Windows and GCC on Ubuntu.

## Installing and consuming

Install the configured build to a prefix:

```powershell
cmake --install build/windows-msvc-release --prefix C:\houio
```

A separate CMake project can then consume HouIO without source-tree paths:

```cmake
find_package(houio 0.2 CONFIG REQUIRED)

target_link_libraries(my_target PRIVATE houio::houio)
```

The install includes the `houio::houio` CMake target, all public headers, and `bin/houio_convert` when tools are enabled. `houio.package_consumer` validates this contract by installing to a clean temporary prefix, configuring an external project, compiling against `GeometryIO.h`, and running a geometry export/import round-trip. The same test runs in CI.

## Supported data model

### Attributes

HouIO models these Houdini attribute domains:

- Point
- Vertex
- Primitive
- Global

Tested numeric storage includes signed Int32, signed Int64, Float16, Float32, and Float64. Numeric loading supports modern tuple/component arrays and validated legacy paged layouts, including constant-page compression and partial final pages. Indexed string tables are expanded and exported for the tested point, primitive, and global cases. Unsigned attribute storage and full semantic metadata are not yet modeled.

### Groups

The Houdini-oriented model preserves unordered point, vertex, and primitive groups as named boolean membership masks. Group masks are validated against the declared domain count during import and export. Ordered group selections are rejected because their semantics are not implemented.

### Primitives

The Houdini-oriented layer currently recognizes:

- `Poly`
- Legacy polygon `run` records
- Houdini 21/22 `Polygon_run` and `PolygonCurve_run` records
- Legacy `Volume`

The simplified `Geometry` class supports one primitive type per object:

- Points
- Lines
- Triangles
- Quads
- Polygons

Conversion to `Geometry` is intentionally render-oriented and lossy. Face-varying vertex attributes are converted into point attributes by duplicating points at discontinuities such as UV seams. Primitive and global attributes, groups, mixed polygon sizes, arbitrary n-gons, and unsupported numeric storage types are not represented by this convenience model. Diagnostics-aware conversion rejects missing `P`, inconsistent domain counts, malformed polygon tables, and out-of-range point references rather than returning partially valid geometry.

## Houdini 21/22 minimal fixture matrix

The generated fixture matrix isolates modern schema behavior into 14 small binary files:

- Empty and point-only geometry
- Point `P`, `Cd`, Float16/Float32/Float64, 32-bit integer, 64-bit integer, and string attributes
- Multi-page numeric attributes with constant-page compression and a partial final page
- Triangle-only, quad-only, mixed-size, and n-gon polygon runs
- Open polygon curves and multiple closed/open primitive records
- Vertex-domain UV seams
- Numeric and string global attributes
- A dense scalar volume spanning multiple tiles, with exact resolution, transform, position, and voxel checks
- Primitive string and integer attributes
- Overlapping point, vertex, and primitive groups

Run the complete matrix with both installed Houdini versions:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\houdini\run_fixture_roundtrips.ps1
```

Houdini 22.0.368 generates the sources by default. HouIO round-trips each file, then Houdini 21.0.631 and 22.0.368 compare exact counts, attribute data type and numeric precision, attribute values, primitive type, open/closed state, point topology, dense-volume resolution, transform, position and voxels, and group membership. The same suite also succeeds when Houdini 21.0.631 generates the sources. Generated files stay under the configured build directory rather than being committed as opaque binary assets. The current fixture matrix has no intentional round-trip losses.

## Houdini 21/22 Crag experiment

The modernization branch includes a reproducible static Crag round-trip:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -Command `
  '& ".\tools\houdini\run_crag_roundtrip.ps1"'
```

The harness:

1. Opens Crag at frame 1.
2. Enables its rest T-pose.
3. Removes the time expression from the animation-frame parameter.
4. Unpacks the 67 packed pieces into polygon geometry.
5. Preserves Crag's primitive `name` string and `piece` integer attributes.
6. Saves a binary `.bgeo` source file.
7. Imports it through HouIO and exports a new binary `.bgeo`.
8. Validates and compares the result in Houdini 21.0.631 and Houdini 22.0.368.

The verified geometry contains 90,085 points, 359,794 vertices, and 89,942 polygons. The output is static and preserves polygon topology, point `P`, vertex `N` and `uv`, primitive string `name`, and primitive integer `piece`. Houdini reports a maximum absolute difference of `0.0` for the floating-point attributes and exact matches for all 89,942 primitive string and integer values.

Houdini 21/22 encode this mesh with `Polygon_run` and run-length vertex counts. HouIO reads both run-length counts and direct per-primitive `nvertices`/`n_v` arrays. The writer emits modern polygon-run records with topology vertex offsets and promotes three-component `P` data to its four-component output representation.

Modern Houdini binary `.bgeo` input is covered for this geometry path. The missing geometry compatibility feature was uniform signed-int8 arrays, used by compact binary `Polygon_run` run-length data. The same parser and writer now operate transparently through the validated `.bgeo.sc` wrapper when C-Blosc is available.

To register the same chain with CTest:

```powershell
cmake --preset windows-msvc-release `
  -DHOUIO_HYTHON_EXECUTABLE="C:\Program Files\Side Effects Software\Houdini 22.0.368\bin\hython.exe"
cmake --build --preset windows-msvc-release
ctest --test-dir build/windows-msvc-release --output-on-failure -R houio.crag
```

An AddressSanitizer preset is available for parser and round-trip debugging:

```powershell
cmake --preset windows-msvc-asan
cmake --build --preset windows-msvc-asan
ctest --preset windows-msvc-asan
```

## Contributing

Before changing schema parsing, add or preserve a fixture that demonstrates the relevant encoding. Format handling is reverse-engineered and small assumptions can affect unrelated files. Semantic loading rejects malformed flattened objects, negative counts, invalid group masks, topology-count mismatches, out-of-range point or polygon topology references, invalid volume dimensions and transforms, mismatched tile counts, malformed tile payloads, and unsupported volume compression modes.

Recommended workflow:

1. Add an ASCII `.geo` fixture for readability.
2. Add the equivalent binary `.bgeo` fixture.
3. Log both through `JSONLogger`.
4. Add assertions for counts, topology, attributes, and values.
5. Implement the smallest schema change required.
6. Verify round-trip behavior where export is supported.

## Further documentation

- [architecture.md](architecture.md) — modules, data flow, abstractions, and architectural risks
- [onboard.md](onboard.md) — environment setup and code-reading path
- [todo.md](todo.md) — prioritized maintenance and compatibility work
