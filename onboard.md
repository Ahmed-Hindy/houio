# Onboarding

This guide is for developers approaching HouIO for the first time, especially those validating it against Houdini 21 or 22 on Windows.

## What to understand first

HouIO is not an HDK plug-in and does not run inside Houdini. It is a standalone C++ library that reads and writes Houdini geometry files.

The repository contains three distinct areas:

1. The C++ `.geo` and `.bgeo` library.
2. A lightweight mesh and dense-field implementation used by examples and convenience APIs.
3. A disconnected Python 2 scene exporter under `scene_exporter/`.

Focus on the first two areas initially. Ignore `HouScene`, `ImportHoudini`, and `scene_exporter/` unless your task explicitly concerns them.

## Current baseline

At the time of this fork:

- The modernization branch targets C++20.
- The supplied fixtures were produced by Houdini `13.0.288`.
- The upstream repository has no license.
- The latest upstream commit is from 2020.
- The CMake build is target-based and uses presets.
- Existing historical tests still provide limited semantic coverage.
- A static Crag `P` and polygon-topology round-trip is validated in Houdini 21.0.631 and 22.0.368.
- Modern Houdini binary `.bgeo` input is validated for the static Crag path.

Do not infer modern format support from a successful parse of the supplied fixtures.

## Recommended Windows toolchain

Use a native MSVC toolchain:

- Windows 10
- Visual Studio 2022 Build Tools or Visual Studio 2022
- Desktop development with C++ workload
- CMake 3.24 or newer
- Ninja
- Houdini 21 and 22 for producing compatibility fixtures

MSVC is preferable to MinGW when HouIO may later be used alongside HDK code or other Houdini-adjacent native libraries. GCC remains part of the portability baseline and is exercised through the `linux-gcc-release` preset and Ubuntu CI.

## Clone and remotes

For this fork, a useful remote arrangement is:

```powershell
git clone https://github.com/Ahmed-Hindy/houio.git
cd houio
git remote add upstream https://github.com/dkoerner/houio.git
git remote -v
```

Expected roles:

```text
origin    Ahmed-Hindy/houio
upstream  dkoerner/houio
```

## First build

Open an MSVC Developer PowerShell or Developer Command Prompt, then configure, build, and test through the checked-in presets:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

The main targets are:

```text
houio                     Static library
houio_test_logger         Parses and logs fixture files
houio_roundtrip_geometry  Imports and exports a geometry file through HouIO
houio_example_readwrite   Demonstrates low-level parsing and writing
```

The logger and clean package-consumer project are registered with CTest.

For a non-Houdini Linux portability check:

```bash
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release
ctest --preset linux-gcc-release
```

### Verify the installed package

The packaging test installs HouIO to a clean prefix, configures a separate CMake project with `find_package(houio 0.2 CONFIG REQUIRED)`, builds it, and runs an export/import round-trip:

```powershell
ctest --test-dir build/windows-msvc-release --output-on-failure -R houio.package_consumer
```

This catches broken install destinations, package version files, exported target names, include directories, and transitive link requirements.

## Run the smoke executable

After the Debug preset build:

```powershell
.\build\windows-msvc-debug\tests\houio_test_logger.exe
```

It should print the contents of:

```text
tests/test_box.bgeo
tests/test_volume.bgeo
tests/test_box.geo
tests/test_volume.geo
```

A successful run only proves that the historical fixtures still parse. It does not verify semantic correctness or modern Houdini compatibility.

## Run the minimal Houdini fixture matrix

The fixture harness generates 12 small Houdini binary files, round-trips them through HouIO, and compares them exactly in Houdini 21 and 22:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\houdini\run_fixture_roundtrips.ps1
```

The matrix covers empty geometry; typed point attributes; triangle, quad, mixed-size, and n-gon polygon runs; open polygon curves; UV seams; multiple primitive records; global and primitive attributes; a dense scalar volume spanning multiple tiles; and overlapping point, vertex, and primitive groups. Validation checks counts, attribute metadata and values, primitive type, open/closed state, point topology, dense-volume resolution, transform, position and voxel values, and every group membership. The suite succeeds with either Houdini 21.0.631 or Houdini 22.0.368 as the generator.

Generated sources, outputs, and `manifest.json` live under:

```text
build/windows-msvc-release/tests/fixtures/
```

The fixture matrix currently has no intentional round-trip losses. Group support is limited to unordered membership masks; ordered selections are rejected explicitly.

## Run the Crag integration experiment

The one-command harness builds HouIO, generates Crag in a static rest T-pose, round-trips it, and validates the binary output in Houdini 21 and 22:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -Command `
  '& ".\tools\houdini\run_crag_roundtrip.ps1"'
```

Default versions:

```text
Generator:  Houdini 22.0.368
Validators: Houdini 21.0.631 and 22.0.368
```

To generate from Houdini 21 instead:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -Command `
  '& ".\tools\houdini\run_crag_roundtrip.ps1" -GeneratorVersion "21.0.631"'
```

The current experiment proves:

- Rest-pose Crag can be made time-independent.
- Houdini 21/22 `Polygon_run` records can be imported.
- 90,085 points, 359,794 vertices, and 89,942 polygons survive the round-trip with exact point topology.
- HouIO binary output loads in Houdini 21.0.631 and 22.0.368.
- Point `P` and vertex `N` and `uv` match the source exactly, with a maximum absolute difference of `0.0`.
- Primitive string `name` and primitive integer `piece` match all 89,942 source values exactly.
- Both run-length and direct per-primitive vertex-count encodings are covered by focused tests.
- Closed `Polygon_run` and open `PolygonCurve_run` state survives export.

To expose the same workflow through CTest, configure with a hython executable:

```powershell
cmake --preset windows-msvc-release `
  -DHOUIO_HYTHON_EXECUTABLE="C:\Program Files\Side Effects Software\Houdini 22.0.368\bin\hython.exe"
cmake --build --preset windows-msvc-release
ctest --test-dir build/windows-msvc-release --output-on-failure -R houio.crag
```

Use the AddressSanitizer preset when changing binary parsing or raw attribute storage:

```powershell
cmake --preset windows-msvc-asan
cmake --build --preset windows-msvc-asan
ctest --preset windows-msvc-asan
```

## Read the code in this order

### 1. `tests/example_readwrite.cpp`

Start here to understand:

- Why Houdini objects appear as flattened arrays.
- How `json::Parser` and `JSONReader` are used.
- How writer events construct Houdini geometry records.

This example is intentionally incomplete, but it establishes the file-shape vocabulary.

### 2. `include/houio/json.h`

Read these types first:

```text
Token
Handler
Parser
Writer
BinaryWriter
ASCIIWriter
Value
Array
Object
JSONReader
JSONLogger
```

Do not try to memorize every binary token. Understand the flow from stream bytes to handler events.

### 3. `src/json.cpp`

Trace:

```text
Parser::parse
Parser::parseStream
Parser::readToken
Parser::readBinaryToken
Parser::readASCIIToken
```

Then follow uniform-array callbacks into `JSONReader`. `ParserLimits` bounds total input bytes, string bytes, uniform-array elements, and nesting depth. Seekable streams are checked up front, streaming inputs are bounded while reading, payload sizes are validated before handler allocation, and complete-document checks reject trailing data.

### 4. `HouGeo::load()` in `src/HouGeo.cpp`

This is the semantic boundary where generic JSON becomes geometry.

Follow the load order:

```text
Root counts
Attributes
Topology
Shared primitive data
Primitives
```

Then inspect:

```text
loadAttribute
loadTopology
loadPrimitive
loadPolyPrimitive
loadPolyPrimitiveRun
loadVolumePrimitive
loadVoxelData
```

### 5. `src/HouGeoIO.cpp`

Trace the public workflows:

```text
import
importGeometry
importVolume
convertToGeometry
xport
makeLog
```

Pay particular attention to assumptions in `convertToGeometry()`. It does not represent arbitrary Houdini geometry.

### 6. `include/houio/HouGeoAdapter.h`

Read this before designing an integration. It is the intended abstraction for exporting a client-owned geometry representation.

### 7. `Geometry`, `Attribute`, and `Field`

Treat these as convenience implementations:

- `Geometry` is a render-oriented mesh.
- `Attribute` is raw tuple storage.
- `Field<T>` is a dense grid.

They are not a complete model of Houdini geometry.

## Debugging a file

When a file fails, capture structured diagnostics first, then use the generic logger when you need to inspect the decoded record stream.

```cpp
houio::DiagnosticList diagnostics;
std::ifstream input("problem.bgeo", std::ios::binary);
houio::HouGeo::Ptr geometry = houio::HouGeoIO::import(&input, &diagnostics);

houio::HouGeoIO::makeLog("problem.bgeo", &std::cout);
```

Parser diagnostics include byte offsets. Semantic diagnostics include paths such as `topology`, `attributes.pointattributes[0]`, or `primitives[2].definition.type`. Categories distinguish malformed input, unsupported input, schema errors, I/O failures, and convenience-conversion warnings.

This separates two failure categories:

### Parser failure

The binary or ASCII JSON layer cannot decode the stream.

Investigate:

- Opening magic bytes
- Token identifiers
- Length encodings
- Uniform-array storage types
- Truncation or outer compression
- Configured `ParserLimits` versus the observed total input, string, array, and nesting sizes
- Undefined shared string-token references

### Schema failure

The stream parses, but `HouGeo::load()` does not recognize or safely interpret the geometry structure.

Investigate:

- Primitive definitions and type names
- Attribute storage and packing
- Topology indirection
- Shared primitive records
- New compression or paging metadata

Preserve the failing fixture before changing code.

## Creating Houdini 21 fixtures

Create small, isolated files rather than exporting a complex production asset.

Prefer pairs of:

```text
case_name.geo
case_name.bgeo
```

Avoid `.bgeo.sc` initially because HouIO does not handle the outer compression wrapper.

Recommended fixture sequence:

1. One point with only `P`.
2. Multiple points with `P`, `Cd`, and an integer attribute.
3. One triangle.
4. One quad.
5. A triangle run.
6. A quad run.
7. A mesh with vertex UV seams.
8. Point and primitive string attributes.
9. Mixed triangles and quads.
10. A dense scalar volume.
11. Packed geometry.
12. VDB geometry.

The final cases are expected to expose unsupported paths. Tests should verify clean diagnostics rather than crashes.

Keep fixtures minimal. Small ASCII files are easier to compare when a Houdini version changes the schema.

## Adding a compatibility test

The desired test pattern is:

1. Parse the ASCII fixture.
2. Parse the binary equivalent.
3. Assert root counts.
4. Assert attribute names, tuple sizes, storage, and values.
5. Assert topology.
6. Assert primitive type and vertex lists.
7. Export when the feature is supported.
8. Re-import the output.
9. Compare semantic data rather than raw bytes.

Raw byte equality is usually inappropriate because token definitions, integer widths, and record ordering may differ while representing the same geometry.

## Important code invariants

### Houdini `P`

The export path expects Houdini's `P` attribute to contain four components. The convenience geometry exporter promotes a three-component position to four components with `w = 1`.

### Topology indirection

A primitive may reference vertex entries, and the topology maps those entries to point indices. Do not assume every primitive index is already a point index. The loader now rejects topology-count mismatches, point references outside the declared point domain, and polygon references outside the topology buffer.

### Malformed input baseline

`houio.malformed_geometry` exercises semantic rejection for odd or duplicate flattened keys, negative domain counts, invalid topology, malformed group masks, unsupported ordered group selections, unsupported primitive records, schema paths, and I/O diagnostics. `houio.binary_json` additionally verifies byte offsets, total-input, string, uniform-array, and nesting limits, truncated reads, declared payload sizes, trailing data, duplicate native map keys, token references, and generic array bounds.

### Point and vertex domains

Houdini point attributes and vertex attributes are distinct. The simplified `Geometry` conversion duplicates points to flatten vertex data into point data.

### Primitive runs

One stored `HouPoly` object may represent multiple logical primitives. `primitivecount()` therefore sums `Primitive::numPrimitives()` rather than returning the size of the primitive vector.

### Uniform arrays

A JSON `Array` may store values in a packed uniform buffer rather than individual `Value` objects. Any new array-processing code must support both representations.

## Known traps

### `importGeometry()` is not a general importer

It selects the first stored primitive representation and only creates line, triangle, or quad geometry when the polygon vertex count is constant.

### `importVolume()` requires a valid dense volume primitive

The diagnostics-aware overload reports empty primitive lists and non-volume first primitives through schema or unsupported-input diagnostics. Dense-volume loading also validates resolution, topology and `P` references, the transform tuple, tile count, tile payload sizes, and compression modes. The historical overload returns null for convenience-import failures.

### Export uses a scoped thread-local binding

Each export owns its writer on the stack. A scoped thread-local pointer connects the legacy helper functions to that writer and is restored after exceptions. Parallel exports are covered when each thread uses an independent stream.

### ASCII geometry export is not implemented

The high-level stream export returns `false` for `binary=false` and writes no partial output. Use binary `.bgeo` output until the geometry serializer is generalized for `ASCIIWriter`.

### Prefer diagnostics-aware import overloads

The historical low-level parser path still throws when no diagnostic list is supplied. Diagnostics-aware `import()`, `importGeometry()`, `importVolume()`, and `convertToGeometry()` overloads capture failures and return null. New import-facing failure paths should preserve categories, parser offsets, and semantic paths instead of printing to standard output.

### Raw attribute memory requires care

`Attribute` uses a byte vector. Append operations now use `std::memcpy`, validate count conversions, and reject invalid indexed raw-pointer access. The legacy typed `get()` and `set()` methods still use reinterpret casts, so tuple types and alignment remain caller responsibilities.

### Warning baseline

MSVC and GCC builds still report intentional compatibility debt. Anonymous unions preserve the public vector, matrix, and color layouts, while the legacy JSON and dense-field paths retain conversion and unreachable-code warnings. Warnings-as-errors stays disabled until those paths are modernized without changing file-format behavior or public layout.

## A safe first contribution

A good first maintenance change is narrow and observable:

1. Remove the duplicate `add_subdirectory(tests)` call.
2. Enable CTest.
3. Convert the historical fixture smoke run into assertions for counts and key attributes.
4. Add CI for MSVC and one non-Windows compiler.
5. Leave schema behavior unchanged.

This establishes a baseline before format work.

## Before opening a pull request

Run or document:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

Also verify:

- No generated build files are tracked.
- New schema behavior has a minimal fixture.
- Binary and ASCII paths are both considered.
- Unsupported files fail cleanly.
- Existing historical fixtures still pass.
- Documentation distinguishes confirmed behavior from assumptions.

## Where to go next

Use [architecture.md](architecture.md) when you need module boundaries and data-flow details.

Use [todo.md](todo.md) to select work that fits the current modernization priorities.
