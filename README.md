# HouIO

HouIO is a small C++20 library for reading and writing Houdini geometry files without linking against Houdini or the Houdini Development Kit.

It implements SideFX's JSON-based geometry encoding used by:

- `.geo` ASCII geometry files
- `.bgeo` binary geometry files

HouIO is a standalone file-format library. It is **not** a Houdini plug-in, does not load `.hip` or `.hipnc` scene files, and does not require a Houdini installation at runtime.

> [!IMPORTANT]
> The repository currently has no license file. Public source availability does not grant permission to redistribute or embed the code. Resolve licensing with the original author before using HouIO in a distributed product.

## Project status

HouIO should currently be treated as an experimental legacy library rather than a production-ready dependency.

- The included geometry fixtures identify themselves as Houdini `13.0.288` files.
- The latest upstream commit is from 2020.
- A static Crag round-trip and an 11-case minimal geometry matrix have been validated with Houdini `21.0.631` and `22.0.368`.
- Polygon and legacy dense-volume paths are the best-developed areas.
- Packed geometry, VDBs, curves, agents, height fields, and most modern primitive types are not supported.
- `.bgeo.sc` compression is not handled.

See [todo.md](todo.md) for the modernization plan.

## Main capabilities

- Parse Houdini ASCII and binary JSON.
- Inspect unknown Houdini geometry structures using a JSON logger.
- Read point, vertex, primitive, and global attribute domains.
- Preserve unordered point, vertex, and primitive groups.
- Read legacy polygon runs, Houdini 21/22 `Polygon_run` records, and open `PolygonCurve_run` records.
- Read legacy dense Houdini volumes, including tiled and constant storage.
- Write point, polygon, and dense-volume `.bgeo` files.
- Convert supported Houdini geometry into a lightweight render-oriented mesh representation.
- Export custom geometry implementations through the `HouGeoAdapter` interface.

## Repository layout

```text
include/houio/       Public C++ headers
include/ttl/         Vendored variant and template utilities
src/                 Library implementation
src/math/            Math implementation
tests/               Unit, packaging, and Houdini integration tests
scene_exporter/      Separate legacy Houdini Python 2 scene exporter
cmake/               CMake package configuration
```

For a module-level description, see [architecture.md](architecture.md).

For a practical introduction to the codebase, see [onboard.md](onboard.md).

## Quick API examples

### Read geometry

```cpp
#include <houio/HouGeoIO.h>

houio::Geometry::Ptr geometry =
    houio::HouGeoIO::importGeometry("mesh.bgeo");

if (!geometry)
{
    // Handle the import failure.
}
```

`importGeometry()` performs a deliberately lossy conversion of the first supported polygon primitive representation into HouIO's simplified `Geometry` class. It requires a point `P` value for every declared point, one fixed polygon size, valid point references, and attribute element counts that match their owner domains. Use the lower-level API when you need to preserve Houdini attribute domains, mixed polygon sizes, unsupported attributes, groups, or multiple primitive records.

### Read the Houdini-oriented representation

```cpp
#include <fstream>
#include <houio/HouGeoIO.h>

std::ifstream input("mesh.bgeo", std::ios::binary);
houio::HouGeo::Ptr houdiniGeometry = houio::HouGeoIO::import(&input);
```

### Write geometry

```cpp
#include <houio/HouGeoIO.h>

houio::Geometry::Ptr geometry = houio::Geometry::createTriangleGeometry();
// Populate P, other attributes, and indices.

bool exported = houio::HouGeoIO::xport("output.bgeo", geometry);
```

The public API uses the historical name `xport` rather than `export`. Stream-based exports are stack-owned and safe to run concurrently when each call uses an independent stream. The `binary=false` path is explicitly unsupported and returns `false` without writing partial output.

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

The original overloads remain available. Parser failures continue to throw when no diagnostic list is supplied, while the diagnostics-aware overloads capture those failures and return null. `importGeometry()`, `importVolume()`, and `convertToGeometry()` also accept diagnostic lists. `importVolume()` now reports an empty primitive list instead of indexing it.

## Building

HouIO builds as a static C++20 library using target-based CMake.

For Windows integration, use an MSVC Developer PowerShell or Developer Command Prompt:

```powershell
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release
```

The release preset uses Ninja, builds the tools and examples, and runs unit, package-consumer, and optional Houdini integration tests. Houdini integration tests are registered when `HOUIO_HYTHON_EXECUTABLE` points to an installed `hython.exe`.

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

`houio.package_consumer` validates this contract by installing to a clean temporary prefix, configuring an external project, building it through `find_package`, and running a geometry export/import round-trip. The same test runs in CI.

## Supported data model

### Attributes

HouIO models these Houdini attribute domains:

- Point
- Vertex
- Primitive
- Global

Numeric support is primarily focused on 32-bit float, 64-bit float, and 32-bit integer storage. String support is partial.

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

Modern Houdini binary `.bgeo` input is now covered for this geometry path. The missing compatibility feature was uniform signed-int8 arrays, used by compact binary `Polygon_run` run-length data. `.bgeo.sc` remains unsupported because HouIO does not implement its outer compression layer.

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
