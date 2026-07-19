# Onboarding

This guide is for developers approaching HouIO for the first time, especially those validating it against Houdini 21 on Windows.

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
- Existing tests still provide limited semantic coverage.
- Houdini 21 and 22 compatibility is under active validation.

Do not infer modern format support from a successful parse of the supplied fixtures.

## Recommended Windows toolchain

Use a native MSVC toolchain:

- Windows 10
- Visual Studio 2022 Build Tools or Visual Studio 2022
- Desktop development with C++ workload
- CMake 3.24 or newer
- Ninja
- Houdini 21 and 22 for producing compatibility fixtures

MSVC is preferable to MinGW when HouIO may later be used alongside HDK code or other Houdini-adjacent native libraries.

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
houio_example_readwrite   Demonstrates low-level parsing and writing
```

The logger is registered with CTest.

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

A successful run only proves that the historical fixtures still parse. It does not verify semantic correctness or Houdini 21 compatibility.

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

Then follow uniform-array callbacks into `JSONReader`.

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

When a file fails, begin with the generic logger rather than the semantic geometry loader.

```cpp
houio::HouGeoIO::makeLog("problem.bgeo", &std::cout);
```

This separates two failure categories:

### Parser failure

The binary or ASCII JSON layer cannot decode the stream.

Investigate:

- Opening magic bytes
- Token identifiers
- Length encodings
- Uniform-array storage types
- Truncation or outer compression

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

A primitive may reference vertex entries, and the topology maps those entries to point indices. Do not assume every primitive index is already a point index.

### Point and vertex domains

Houdini point attributes and vertex attributes are distinct. The simplified `Geometry` conversion duplicates points to flatten vertex data into point data.

### Primitive runs

One stored `HouPoly` object may represent multiple logical primitives. `primitivecount()` therefore sums `Primitive::numPrimitives()` rather than returning the size of the primitive vector.

### Uniform arrays

A JSON `Array` may store values in a packed uniform buffer rather than individual `Value` objects. Any new array-processing code must support both representations.

## Known traps

### `importGeometry()` is not a general importer

It selects the first stored primitive representation and only creates line, triangle, or quad geometry when the polygon vertex count is constant.

### `importVolume()` assumes a primitive exists

The function currently accesses the first primitive without first validating that the primitive list is non-empty.

### Export is not thread-safe

`HouGeoIO` uses static mutable writer state. Do not run concurrent exports through the current API.

### The `binary` flag is misleading

The high-level stream export currently creates `BinaryWriter` in both branches.

### Error reporting is inconsistent

Failures may return null, print to standard output, or throw a generic exception. Add context when touching a failure path.

### Raw attribute memory requires care

`Attribute` uses a byte vector and reinterpret casts. Tuple sizes, element types, alignment, and bounds are mostly caller responsibilities.

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
