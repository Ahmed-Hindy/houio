# Architecture

HouIO is organized around a standalone C++20 file-format library, a path-based I/O facade, a command-line converter, and an optional Houdini Python integration layer.

The minimum supported Houdini version is 20.0.

## System overview

```text
.geo / .bgeo / .bgeo.sc
          |
          v
   GeometryIO facade
          |
          +--> SCF decompression when required
          |
          v
     JSON parser
          |
          v
   generic JSON tree
          |
          v
      HouGeo model
          |
          +--> simplified Geometry
          +--> dense ScalarField objects
          +--> caller-defined HouGeoAdapter
```

Writes follow the reverse path. Every operation is synchronous and owns its parser, writer, diagnostics, and temporary buffers.

## Modules

| Module | Files | Responsibility |
| --- | --- | --- |
| Public I/O | `GeometryIO.h`, `GeometryIO.cpp` | Format detection, path I/O, owned results, diagnostics, and SCF routing |
| JSON format | `json.h`, `json.cpp` | ASCII and binary JSON parsing, handlers, writers, limits, and byte offsets |
| Houdini model | `HouGeo.h`, `HouGeo.cpp` | Attributes, topology, groups, polygons, curves, and dense volumes |
| Adapter API | `HouGeoAdapter.h`, `HouGeoAdapter.cpp` | Read-only export contract for caller-owned geometry |
| Compatibility API | `HouGeoIO.h`, `HouGeoIO.cpp` | Stream operations and thin source-compatible wrappers |
| Simplified mesh | `Geometry.h`, `Geometry.cpp` | Render-oriented points, lines, triangles, quads, and polygons |
| Attribute storage | `Attribute.h`, `Attribute.cpp` | Fixed-width tuple storage backed by contiguous bytes |
| Dense fields | `Field.h`, `Field.cpp` | Dense scalar/vector grids, transforms, and sampling |
| SCF | `Scf.h`, `Scf.cpp` | SideFX SCF framing and dynamic C-Blosc integration |
| Houdini bridge | `python/houio_hom` | Package workflows, in-memory BGEO exchange, and VDB conversion |
| Converter | `tools/houio_convert.cpp` | Path-to-path conversion using the public API |

## GeometryIO

`GeometryIO` is the preferred entry point.

Read methods return `GeometryReadResult<T>`:

- `value` owns the result.
- `diagnostics` contains errors, warnings, schema paths, and byte offsets.
- `succeeded` distinguishes valid empty results from failure.
- `operator bool()` is true only when the operation succeeded without errors.

Write methods return `GeometryWriteResult` with the same diagnostics model.

Supported path operations:

- `readHouGeo()` for the Houdini-oriented representation
- `readGeometry()` for the simplified mesh
- `readVolume()` for the first dense scalar volume
- `readVolumes()` for all dense scalar volumes
- `writeHouGeo()` for adapter-backed output
- `writeGeometry()` for the simplified mesh
- `writeVolume()` for one dense scalar volume

Format detection uses readable file signatures first and extensions as a fallback.

## JSON parser

The parser supports Houdini ASCII JSON and binary JSON tokens.

Each parse owns:

- Input stream state
- String table
- Nesting stack
- Byte offset
- Configured limits
- Diagnostic output

Safety limits cover:

- Full-file size
- String length
- Uniform-array element count
- Nesting depth
- Fixed-size reads
- Length conversion and multiplication overflow

The parser rejects malformed flattened objects, invalid string references, truncated payloads, unsupported storage tokens, and duplicate keys.

Handlers separate token decoding from consumers:

- `JSONReader` builds the generic tree.
- `JSONLogger` exposes parser events for diagnostics and testing.
- Writers serialize ASCII or binary JSON.

## Houdini object decoding

Houdini geometry objects are represented as flattened key/value arrays. `Value::toObject()` validates the array before converting it into an object:

- The item count must be even.
- Keys must be strings.
- Keys must be unique.

`HouGeo::load()` then performs schema-aware decoding.

### Attributes

Each attribute has:

- Domain: point, vertex, primitive, or global
- Name
- Tuple size
- Numeric storage or expanded strings
- Element count validated against its domain

Supported numeric storage:

- Signed Int32
- Signed Int64
- Float16
- Float32
- Float64

Numeric payloads may use tuple arrays, component arrays, or paged arrays. Paged decoding validates packing coverage, page sizes, constant pages, partial final pages, and exact payload consumption.

String attributes decode indexed string tables into per-element values.

### Topology

Topology stores vertex-to-point indices. Every point reference is validated against the declared point count.

### Groups

Point, vertex, and primitive groups use boolean membership masks. Mask size must match the corresponding domain. Ordered selections are rejected because their ordering semantics are not implemented.

### Primitives

Supported primitive records:

- `Poly`
- `Polygon_run`
- `PolygonCurve_run`
- Dense scalar `Volume`

Compact binary aliases are normalized during decoding. Polygon runs support run-length counts and direct per-primitive vertex counts. All primitive ranges are checked against topology and declared primitive totals.

### Dense volumes

Dense volumes preserve:

- Resolution
- Local-to-world transform
- Position
- Tile data
- Constant tiles
- Visualization mode
- Iso value
- Density

Allocation and indexing are validated before voxel storage is created.

## Export model

`HouGeoAdapter` exposes geometry without requiring callers to use HouIO containers.

The exporter requests:

- Domain counts
- Attribute metadata and contiguous values
- Topology indices
- Primitive records
- Group masks

Caller-owned pointers are consumed only during the synchronous write call. HouIO does not retain them.

Every export owns a `BinaryWriter` and `ExportContext` on the stack. Independent files or streams do not share writer state and can be processed concurrently.

Closed polygons are written as `Polygon_run`; open polygons use `PolygonCurve_run`. Three-component floating-point `P` values are emitted with a fourth component of `1` where required by the file representation.

## SCF boundary

`.bgeo.sc` wraps a complete binary BGEO payload in SideFX SCF blocks.

SCF handling is isolated from JSON and geometry decoding:

1. Read and validate the SCF header.
2. Enforce compressed and decompressed size limits.
3. Dynamically resolve C-Blosc.
4. Decompress into an owned buffer.
5. Parse the embedded binary BGEO.

Writes serialize binary BGEO first, then compress it into SCF blocks.

C-Blosc resolution uses, in order:

- Explicit API path
- `HOUIO_BLOSC_LIBRARY`
- Active Houdini installation paths
- Platform library names

## Simplified Geometry

`Geometry` is a convenience representation for rendering and interchange. It supports one primitive mode per object.

Vertex attributes are converted to point attributes by duplicating points at discontinuities such as UV seams. This preserves visible mesh data but is not a lossless representation of every Houdini domain.

Use `HouGeo` when primitive attributes, groups, mixed records, or exact domain separation must be retained.

## Attribute storage

`Attribute` stores fixed-width tuples in `std::vector<std::byte>`-equivalent contiguous storage.

Append operations:

- Validate tuple width and element counts
- Check size conversions
- Copy data with `std::memcpy`
- Reject null storage for non-empty input
- Reject indexed access outside the stored range

The typed accessors remain compatibility APIs and require the caller to request a type matching the declared storage and alignment expectations.

## Houdini bridge

The Houdini integration layer does not load a CPython extension or HDK plug-in. It uses HOM and starts `houio_convert` as a subprocess with a finite timeout.

Supported bridge workflows:

- `.geo`, `.bgeo`, and `.bgeo.sc` through `hou.Geometry`
- In-memory uncompressed BGEO exchange
- Float SDF and Fog VDB conversion to dense volumes
- Restoration of VDB class through `houio_vdb_class`
- Python SOP round trips
- Shelf-tool file conversion
- Package diagnostics

Sparse VDB topology is not preserved by the C++ model. Densification is explicit and may significantly increase memory use.

## Testing architecture

The default CTest suite covers:

- JSON token decoding
- Parser mutation corpus
- Attributes and numeric storage
- Paged attribute layouts
- Polygon and curve records
- Groups
- Malformed geometry
- Dense volumes
- Conversion safety
- Export safety and concurrency
- Path I/O and SCF
- Installed CMake package consumption

Additional configurations provide:

- MSVC AddressSanitizer
- GCC UndefinedBehaviorSanitizer
- Clang libFuzzer
- Houdini package validation
- Generated fixture round trips
- Static Crag comparison

Generated Houdini fixtures stay in the build tree. They are described by manifests and compared inside installed Houdini versions rather than committed as opaque application assets.

## Design constraints

Current constraints that affect future work:

- The generic JSON tree and `HouGeo` model duplicate some input memory.
- The simplified mesh cannot represent every Houdini domain losslessly.
- Native sparse OpenVDB storage is outside the standalone library.
- Unsigned attribute storage and complete semantic metadata are not modeled.
- Some public math layouts use anonymous unions for source compatibility.
- Compatibility wrappers expose multiple error styles; new path APIs should use owned result objects.

## Direction

Prefer changes that:

1. Keep `GeometryIO` as the stable path facade.
2. Add fixture-backed behavior before expanding schema support.
3. Preserve exact storage and domain information in `HouGeo`.
4. Keep optional Houdini and C-Blosc dependencies outside the core parser.
5. Remove unsafe typed access incrementally without changing supported file behavior.
6. Measure memory and performance before introducing streaming APIs.
