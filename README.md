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
- A static Crag `P` and polygon-topology round-trip has been validated with Houdini `21.0.631` and `22.0.368`.
- Polygon and legacy dense-volume paths are the best-developed areas.
- Packed geometry, VDBs, curves, agents, height fields, and most modern primitive types are not supported.
- `.bgeo.sc` compression is not handled.

See [todo.md](todo.md) for the modernization plan.

## Main capabilities

- Parse Houdini ASCII and binary JSON.
- Inspect unknown Houdini geometry structures using a JSON logger.
- Read point, vertex, primitive, and global attribute domains.
- Read legacy polygon runs and Houdini 21/22 `Polygon_run` records.
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
tests/               Smoke-test executables and Houdini 13 fixtures
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

`importGeometry()` converts the first supported polygon primitive representation into HouIO's simplified `Geometry` class. Use the lower-level API when you need to preserve Houdini attribute domains or inspect multiple primitives.

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

The public API uses the historical name `xport` rather than `export`.

### Inspect a file's structure

```cpp
#include <iostream>
#include <houio/HouGeoIO.h>

houio::HouGeoIO::makeLog("mesh.bgeo", &std::cout);
```

The logger is useful because Houdini geometry uses flattened key/value arrays and has changed across Houdini versions.

## Building

HouIO builds as a static C++20 library using target-based CMake.

For Windows integration, use an MSVC Developer PowerShell or Developer Command Prompt:

```powershell
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release
```

The release preset uses Ninja, builds the examples, and registers the historical logger executable with CTest. Houdini integration tests are registered when `HOUIO_HYTHON_EXECUTABLE` points to an installed `hython.exe`.

## Supported data model

### Attributes

HouIO models these Houdini attribute domains:

- Point
- Vertex
- Primitive
- Global

Numeric support is primarily focused on 32-bit float, 64-bit float, and 32-bit integer storage. String support is partial.

### Primitives

The Houdini-oriented layer currently recognizes:

- `Poly`
- Legacy polygon `run` records
- Houdini 21/22 `Polygon_run` records
- Legacy `Volume`

The simplified `Geometry` class supports one primitive type per object:

- Points
- Lines
- Triangles
- Quads
- Polygons

Conversion to `Geometry` is intentionally render-oriented. Face-varying vertex attributes are converted into point attributes by duplicating points at discontinuities such as UV seams.

## Houdini 21/22 Crag experiment

The modernization branch includes a reproducible static Crag round-trip:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -Command `
  '& ".\tools\houdini\run_crag_roundtrip.ps1"'
```

The harness:

1. Opens Crag at frame 1.
2. Enables its rest T-pose.
3. removes the time expression from the animation-frame parameter.
4. Unpacks the 67 packed pieces into polygon geometry.
5. Removes indexed primitive string attributes that HouIO does not yet preserve correctly.
6. Saves a binary `.bgeo` source file.
7. Imports it through HouIO and exports a new binary `.bgeo`.
8. Validates the result in Houdini 21.0.631 and Houdini 22.0.368.

The verified geometry contains 90,085 points, 359,794 vertices, and 89,942 polygons. The output is static and preserves `P` plus polygon topology. Vertex `N` and `uv`, and primitive `name` and `piece`, are intentionally not part of this first preservation milestone.

Houdini 21/22 encode this mesh with `Polygon_run` and run-length vertex counts. HouIO now reads that record and promotes three-component `P` data to the four-component representation used by its legacy writer.

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

Before changing schema parsing, add or preserve a fixture that demonstrates the relevant encoding. Format handling is reverse-engineered and small assumptions can affect unrelated files.

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
