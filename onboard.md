# Developer onboarding

HouIO is a C++20 custom Houdini geometry writer/reader with a primary CLI and an optional Houdini 20.0+ package.

Start with the public `Writer` facade for serialization and `GeometryIO` for reading or advanced format control. Move into parser or schema internals only when the task requires it.

## Requirements

Windows development:

- Visual Studio 2022
- CMake 3.24 or newer
- Ninja through the Visual Studio CMake installation
- Houdini 20.0 or newer for integration tests

Linux development:

- GCC or Clang with C++20 support
- CMake 3.24 or newer
- Ninja

## Build and test

The Windows developer entry point exposes the maintained workflows:

```powershell
.\tools\dev.ps1 help
.\tools\dev.ps1 build
.\tools\dev.ps1 test
.\tools\dev.ps1 fixtures
.\tools\dev.ps1 package
.\tools\dev.ps1 benchmarks
.\tools\dev.ps1 validate-all
```

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

Windows AddressSanitizer:

```powershell
cmake --preset windows-msvc-asan
cmake --build --preset windows-msvc-asan
ctest --preset windows-msvc-asan
```

Linux GCC:

```bash
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release
ctest --preset linux-gcc-release
```

Linux UndefinedBehaviorSanitizer:

```bash
cmake --preset linux-gcc-ubsan
cmake --build --preset linux-gcc-ubsan
ctest --preset linux-gcc-ubsan
```

## Recommended reading order

1. `include/houio/Writer.h`
2. `tests/test_writer.cpp`
3. `include/houio/GeometryIO.h`
4. `include/houio/HomManifest.h`
5. `include/houio/HouGeo.h`
6. `src/HouGeo.cpp`
7. `include/houio/json.h`
8. `src/json.cpp`
9. `include/houio/HouGeoAdapter.h`
10. `python/houio_hom/manifest.py`
11. `python/houio_hom/bridge.py`

This sequence moves from the supported public facade into schema decoding, binary parsing, export adapters, and Houdini integration.

## Core API model

Use `Writer` as the primary custom serialization facade:

```cpp
houio::GeometryWriteOptions options;
options.atomicReplace = true;
const houio::WriteResult result = houio::Writer::write(
    "asset.bgeo.sc",
    geometry,
    options);
```

Use `GeometryIO` for path-based reads and lower-level format operations:

```cpp
const auto result = houio::GeometryIO::readHouGeo("asset.bgeo.sc");
if (!result)
{
    for (const houio::Diagnostic& diagnostic : result.diagnostics)
    {
        // Report diagnostic details.
    }
    return;
}
```

Choose the representation deliberately. `<houio/GeometryModels.h>` also exposes the intention-revealing aliases shown in parentheses:

- `HouGeo` (`HoudiniGeometry`): Houdini-oriented domains, records, attributes, and groups
- `Geometry` (`SimplifiedMesh`): simplified render-oriented mesh
- `ScalarField`: dense scalar volume
- `HouGeoAdapter`: caller-defined export source

## Debugging a file

Start with the primary CLI:

```powershell
.\build\windows-msvc-debug\houio.exe inspect input.bgeo --json
.\build\windows-msvc-debug\houio.exe validate input.bgeo
.\build\windows-msvc-debug\houio.exe write input.bgeo output.bgeo.sc
```

`houio_convert.exe input output` remains available for compatibility.

Use the inspection tools when you need parser or schema detail:

```powershell
.\build\windows-msvc-debug\tests\houio_inspect_json.exe input.bgeo
.\build\windows-msvc-debug\tests\houio_inspect_attributes.exe input.bgeo
```

Classify failures before changing code:

- **I/O**: path, permissions, or stream state
- **Container**: SCF framing or C-Blosc resolution
- **Parser**: malformed token, length, nesting, or string reference
- **Schema**: invalid geometry structure or domain count
- **Unsupported**: recognized data that HouIO does not model
- **Conversion**: data cannot be represented by the simplified mesh

Diagnostics should include the smallest useful schema path and a byte offset when available.

## Adding parser coverage

For a parser bug:

1. Reduce the input to the smallest deterministic byte sequence.
2. Add it to `tests/test_binary_json.cpp` or `tests/parser_corpus.cpp`.
3. Assert the diagnostic category and failure boundary.
4. Run Debug, ASan, and UBSan configurations.
5. Keep semantic geometry changes out of the parser-only commit.

Parser invariants:

- Every fixed-size read checks the stream result.
- Negative lengths are rejected before conversion.
- Multiplication and narrowing are checked.
- String references are validated before lookup.
- Flattened objects require string keys and unique pairs.
- Configured limits are enforced before allocation.

## Adding geometry coverage

For a schema or compatibility change:

1. Generate a minimal fixture with Houdini 20.0 or newer.
2. Record the Houdini build and generation parameters.
3. Assert exact counts, types, values, topology, and groups.
4. Add malformed variants when the new record introduces ranges or indexing.
5. Round-trip the fixture through HouIO.
6. Validate the result inside Houdini.

Generated fixtures belong under the build directory, not in the source tree.

Run the maintained fixture matrix for Houdini 20.0.653, 20.5.410, 21.0.631, and 22.0.368:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\houdini\run_fixture_roundtrips.ps1
```

Use `-GeneratorVersion 20.0.653` when validating Houdini 20.0 as the fixture producer.

Run the Crag integration test:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\houdini\run_crag_roundtrip.ps1
```

## Houdini package development

Build the package:

```powershell
cmake --build --preset windows-msvc-release --target houio_houdini_package
```

Extract and install it:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\install_houdini_package.ps1 `
  -Install
```

The default installer targets Houdini 20.0, 20.5, 21.0, and 22.0. Validate the generated archive across the maintained builds with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\houdini\run_package_matrix.ps1
```

Inside a SOP network, verify:

1. **Package Diagnostics** resolves package, converter, and Blosc paths.
2. **HouIO Round Trip** creates a connected Python SOP.
3. The node cooks without errors or warnings.
4. Point and primitive counts match the source.
5. Disabling the node passes geometry through unchanged.
6. **Convert Geometry File** produces a readable `.bgeo` or `.bgeo.sc` file.

Headless package validation:

```powershell
cmake -DHOUIO_HOUDINI_PACKAGE_ARCHIVE=<archive> `
  -DHOUIO_HOUDINI_PACKAGE_EXTRACT_DIR=<temp-dir> `
  -DHOUIO_HYTHON_EXECUTABLE=<hython.exe> `
  -DHOUIO_HOUDINI_PACKAGE_TEST_SCRIPT=<test_houdini_package.py> `
  -P tests/run_houdini_package_test.cmake
```

See [Houdini integration on Windows](docs/houdini-windows.md) for the complete setup.

## Important invariants

### Ownership

Read results own their geometry and diagnostics. No result refers to the input stream or parser tree after the call returns.

### Export lifetime

Adapter pointers are consumed synchronously. The caller must keep exposed storage valid until the write returns.

### Concurrency

Independent exports do not share process-global or thread-local writer state.

### Attribute domains

Point, vertex, primitive, and global element counts must match the declared geometry domains.

### Topology

Topology values are point indices. Primitive records refer to topology ranges, not directly to point arrays.

### Position attribute

`P` is required for simplified mesh conversion. The Houdini-oriented model may contain valid data without a simplified `Geometry` result. Use `HouGeoIO::convertToGeometryResult` when a caller needs explicit split, skipped-attribute, dropped-group, winding, open-polygon closure-loss, and failure diagnostics.

### Curves

`CurvePrimitive` and `HouCurve` preserve NURBS and Bezier basis metadata, topology vertices, closure, order, knots, and NURBS endpoint interpolation. Rational weights remain ordinary `Pw` point attributes. Direct HOM extraction supports Bezier curves; NURBS extraction is rejected because HOM does not expose the serialized endpoint-interpolation flag, so use file round trips or explicit manifests for exact NURBS data.

### Quadrics

`QuadricPrimitive` is the common native-record base for `SpherePrimitive` and `TubePrimitive`. `HouSphere` and `HouTube` preserve one topology vertex and the exact Houdini 3×3 transform; tubes also preserve cap state and taper. Direct HOM extraction and explicit `sphere`/`tube` manifests use those native fields without tessellation.

### Volumes

`readVolume()` returns the first dense scalar volume and warns when more are present. Use `readVolumes()` when every volume must be retained.

### Packed primitives

Embedded `PackedGeometry` records preserve their shared HouGeo payload, topology vertex, pivot, transform, viewport LOD, and instancing/folder flags. Named `PackedFragment` records additionally preserve fragment attribute/name and local cached bounds. External `PackedDisk` records preserve the authored filename, expansion frame/policy, transform metadata, and flags without dereferencing the target. `PackedDiskSequence` records preserve their explicit sample list, fractional index, wrap mode, transform metadata, and point-instancing flag. Both are supported by direct HOM extraction. Direct HOM extraction of an existing `hou.PackedFragment` remains unavailable because HOM does not expose its embedded source detail.

### VDB

The Houdini-oriented model preserves existing native VDB primitive payloads opaquely and losslessly during file round trips. `SparseFloatGrid`, `SparseInt32Grid`, and `SparseVec3fGrid` provide dependency-neutral sparse voxel and active-tile editing through one typed value-topology implementation. An opt-in OpenVDB build adds native `.vdb` FloatGrid/Int32Grid/Vec3SGrid I/O and constructs Houdini-native VDB primitive payloads. Live HOM extraction remains Float-only and refuses ambiguous activity instead of flattening or guessing.

### SCF

`.bgeo.sc` requires a compatible C-Blosc runtime. Prefer an explicit path in controlled deployments.

## Pull request checklist

Before opening a pull request:

- Run the relevant full CTest preset.
- Run sanitizer coverage for parser, indexing, or storage changes.
- Add deterministic tests for changed behavior.
- Keep generated files out of source control.
- Confirm `git diff --check` is clean.
- Document the exact Houdini build used for DCC validation.
- Keep formatting-only changes separate from behavior changes unless the file is being rewritten intentionally.
- Update public documentation when support, limitations, or package behavior changes.

## Project documents

- [README](README.md)
- [Architecture](architecture.md)
- [Contributing](CONTRIBUTING.md)
- [Compatibility matrix](docs/compatibility.md)
- [Command-line interface](docs/cli.md)
- [Fixture generation and validation](docs/fixtures.md)
- [Performance baselines](docs/benchmarks.md)
- [Experimental field persistence format](docs/field-format.md)
- [Versioning and release policy](docs/versioning.md)
- [Houdini package](docs/houdini-package.md)
- [Houdini integration on Windows](docs/houdini-windows.md)
- [Security policy](SECURITY.md)
- [Roadmap](todo.md)
