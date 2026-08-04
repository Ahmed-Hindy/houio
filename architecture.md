# Architecture

HouIO is organized around a standalone C++20 file-format library, a path-based I/O facade, one command-line application, and an optional Houdini Python integration layer.

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
| Houdini model | `HouGeo.h`, `HouGeo.cpp` | Core object state, attributes, topology, groups, and schema dispatch |
| Attribute schema loading | `HouGeoAttributeSchema.cpp` | Numeric, string, dictionary, and group record decoding into the faithful model |
| Attribute payload loading | `HouGeoAttributeLoad.cpp` | Paged integer expansion and typed numeric component storage for faithful attributes |
| Packed schema loading | `HouGeoPackedLoad.cpp` | Embedded geometry, fragment, disk, and disk-sequence record decoding |
| Polygon schema loading | `HouGeoPolygonLoad.cpp` | Direct polygons, legacy polygon entries, and compact polygon-run decoding |
| Primitive schema loading | `HouGeoPrimitiveLoad.cpp` | Native VDB, Sphere, Tube, NURBS, and Bezier record decoding |
| Dense-volume loading | `HouGeoVolumeLoad.cpp` | Volume transforms, shared/inline voxel payloads, tiled validation, and voxel expansion |
| HOM manifest decoding | `HomManifest.cpp`, `HomManifestAttributes.cpp`, `HomManifestPrimitives.cpp`, `HomManifestVdb.cpp` | Manifest orchestration, attribute/group domains, primitive families, and sparse VDB records |
| Adapter API | `HouGeoAdapter.h`, `HouGeoAdapter.cpp` | Read-only export contract for caller-owned geometry |
| Compatibility I/O | `HouGeoIO.h`, `HouGeoIO.cpp` | Stream operations, faithful export, and thin source-compatible wrappers |
| Model adaptation | `HouGeoAdapt.cpp` | Simplified geometry and dense-field adaptation into `HouGeo` |
| Simplified mesh | `Geometry.h`, `Geometry.cpp` | Render-oriented points, lines, triangles, quads, polygons, and numeric attribute domains |
| Attribute storage | `Attribute.h`, `Attribute.cpp` | Fixed-width tuple storage backed by contiguous bytes |
| Dense fields | `Field.h`, `Field.cpp` | Dense scalar/vector grids, transforms, and sampling |
| SCF | `Scf.h`, `Scf.cpp` | SideFX SCF framing and dynamic C-Blosc integration |
| Houdini bridge | `python/houio_hom` | Package workflows, in-memory BGEO exchange, and VDB conversion |
| Command-line application | `tools/houio.cpp` | Writing, path conversion, inspection, validation, capabilities, and diagnostics |

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

- `JSONReader` builds the generic tree, retains binary numeric uniform arrays at their encoded widths, and compacts homogeneous ordinary numeric arrays with at least 64 elements into contiguous storage when the array closes.
- `JSONLogger` exposes parser events for diagnostics and testing.
- Writers serialize ASCII or binary JSON; binary tree rewrites and HouGeo payload exports preserve retained or compacted numeric tokens and raw storage directly, while ASCII output expands them to the established scalar representation.

A full HouGeo-specific parser handler is intentionally deferred. The retained tree provides flattened-record validation, arbitrary field ordering, recursive shared data, and one path-aware schema decoder; numeric payload amplification is mitigated within that pipeline. See [Direct semantic-handler evaluation](docs/direct-semantic-handler.md).

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

When a tuple or component array already uses the exact destination numeric storage token, attribute loading copies its retained bytes directly into contiguous or strided attribute storage. Mismatched storage continues through the established scalar conversion and validation path.

Supported numeric storage:

- Unsigned UInt8
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

Primitive dispatch remains in `HouGeo.cpp`. Curve, quadric, and native VDB schema decoding is isolated in `HouGeoPrimitiveLoad.cpp`; additional primitive-family extractions should follow this boundary rather than expanding the core translation unit.

Supported primitive records:

- `Poly`
- `Polygon_run`
- `PolygonCurve_run`
- `NURBCurve`
- `BezierCurve`
- `Sphere`
- `Tube`
- Dense scalar `Volume`

Compact binary aliases are normalized during decoding. Polygon runs support run-length counts and direct per-primitive vertex counts. Curve records preserve basis family, topology vertices, closure, order, knots, NURBS endpoint interpolation, and rational `Pw` point weights. Quadric records preserve their topology vertex and exact 3×3 transform; tubes additionally preserve cap state and taper. All primitive ranges are checked against topology and declared primitive totals.

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

Closed polygons are written as `Polygon_run`; open polygons use `PolygonCurve_run`. NURBS and Bezier records are written as distinct curve primitives rather than being flattened into polygon curves. Sphere and Tube records remain native quadrics rather than being tessellated. Three-component floating-point `P` values are emitted with a fourth component of `1` where required by the file representation.

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

`Geometry` is a convenience representation for rendering and interchange. It supports one primitive mode per object and separate numeric point, vertex, primitive, and global attribute maps.

Supported face-varying data remains in the vertex domain, so UV seams and vertex normals do not require point duplication. Primitive attributes are preserved only when the selected primitive run covers the complete source primitive domain; partial mappings are skipped and reported.

Use `HouGeo` when strings, dictionaries, groups, mixed records, curve basis metadata, quadric transforms, or complete Houdini metadata must be retained. The simplified `Geometry` model does not implicitly tessellate NURBS, Bezier, Sphere, or Tube records.

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

The primary Houdini integration layer does not load a CPython extension. It uses HOM, writes the HouIO-owned manifest boundary, and starts the `houio` CLI as a subprocess with a finite timeout. The optional native HDK ROP is isolated behind a C ABI and consumes the same core writer implementation.

Supported bridge workflows:

- `.geo`, `.bgeo`, and `.bgeo.sc` through `hou.Geometry`
- In-memory uncompressed BGEO exchange
- Direct Bezier, Sphere, and Tube extraction through the HouIO-owned manifest boundary
- File and explicit-manifest preservation of NURBS curves
- Float SDF and Fog VDB conversion to dense volumes
- Restoration of VDB class through `houio_vdb_class`
- Python SOP round trips
- Shelf-tool file conversion
- Package diagnostics

The compatibility bridge may densify supported VDBs explicitly, while the faithful Houdini-oriented model preserves opaque native payloads and offers optional typed sparse-grid construction. Direct NURBS HOM extraction is rejected because HOM does not expose the serialized endpoint-interpolation flag.

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
- Dependency-neutral sparse FloatGrid, Int32Grid, and Vec3fGrid editing is part of the standalone library. Native `.vdb` I/O and Houdini-native VDB payload generation are isolated behind the optional OpenVDB backend; exact live-HOM extraction currently remains limited to scalar Float VDBs.
- Unsigned attribute storage is not modeled; attribute definition scope and nested semantic `options` metadata are preserved by the Houdini-oriented model.
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
