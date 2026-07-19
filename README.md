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
- Houdini 21 and 22 compatibility is being established on the modernization branch.
- Polygon and legacy dense-volume paths are the best-developed areas.
- Packed geometry, VDBs, curves, agents, height fields, and most modern primitive types are not supported.
- `.bgeo.sc` compression is not handled.

See [todo.md](todo.md) for the modernization plan.

## Main capabilities

- Parse Houdini ASCII and binary JSON.
- Inspect unknown Houdini geometry structures using a JSON logger.
- Read point, vertex, primitive, and global attribute domains.
- Read polygon primitives and polygon runs.
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

The release preset uses Ninja, builds the examples, and registers the historical logger executable with CTest. The test currently proves that the Houdini 13 fixtures still parse; assertion-based semantic and Houdini 21/22 compatibility tests remain to be added.

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
- Polygon `run` records
- Legacy `Volume`

The simplified `Geometry` class supports one primitive type per object:

- Points
- Lines
- Triangles
- Quads
- Polygons

Conversion to `Geometry` is intentionally render-oriented. Face-varying vertex attributes are converted into point attributes by duplicating points at discontinuities such as UV seams.

## Houdini 21 notes

Compatibility with Houdini 21 must be validated using newly generated fixtures before relying on the library.

When creating fixtures, prefer uncompressed `.geo` and `.bgeo` files. Do not use `.bgeo.sc` during initial testing because HouIO does not implement the outer compression layer.

Start with simple cases:

1. Points with `P`, `Cd`, and integer attributes.
2. Triangle-only meshes.
3. Quad-only meshes.
4. Vertex UV seams.
5. String attributes.
6. Mixed triangle and quad meshes.
7. Dense volumes.
8. Modern primitive types expected to fail cleanly.

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
