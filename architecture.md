# Architecture

This document describes HouIO's current architecture as implemented in the repository. It separates the core file-format code from convenience data structures and abandoned experimental components.

## System overview

HouIO has five main standalone layers plus an optional Houdini bridge:

```text
.geo / .bgeo / .bgeo.sc
      │
      ▼
Format detection and optional SCF/Blosc decompression
      │
      ▼
JSON token parser and writers
      │
      ▼
Generic JSON value tree
      │
      ▼
Houdini geometry schema model
      │
      ├────────► Simplified mesh representation
      │
      └────────► Dense field representation
```

The reverse flow writes supported geometry back to Houdini binary JSON:

```text
Geometry, Field<T>, or a custom HouGeoAdapter
      │
      ▼
GeometryIO / HouGeoIO export orchestration
      │
      ▼
BinaryWriter
      │
      ├────────► raw .bgeo
      │
      └────────► SCF block compression ────────► .bgeo.sc
```

Inside Houdini, the separate HOM bridge handles formats that are intentionally not linked into the standalone C++ library:

```text
.vdb / Houdini SOP geometry
      │
      ▼
houio_hom (hou.Geometry + Convert VDB SOP verb)
      │
      ├────────► uncompressed bgeo bytes
      └────────► houio_convert subprocess
```

## Module map

| Module | Primary files | Responsibility |
| --- | --- | --- |
| Binary JSON | `include/houio/json.h`, `src/json.cpp` | Tokenize, parse, log, and write Houdini ASCII and binary JSON. |
| Houdini adapter contracts | `include/houio/HouGeoAdapter.h`, `src/HouGeoAdapter.cpp` | Define the abstract geometry interface consumed by the writer. |
| Houdini geometry model | `include/houio/HouGeo.h`, `src/HouGeo.cpp` | Interpret Houdini's geometry schema and store attributes, groups, topology, polygons, and volumes. |
| Path-based I/O facade | `include/houio/GeometryIO.h`, `src/GeometryIO.cpp` | Detect containers, own diagnostics/results, decode SCF, and coordinate path reads/writes. |
| Legacy and stream I/O | `include/houio/HouGeoIO.h`, `src/HouGeoIO.cpp` | Parse caller-owned streams, adapt convenience models, and preserve compatibility APIs. |
| SCF wrapper | `src/Scf.h`, `src/Scf.cpp` | Validate SideFX SCF framing and dynamically call C-Blosc for block compression/decompression. |
| Simplified geometry | `include/houio/Geometry.h`, `src/Geometry.cpp` | Store render-oriented points, indices, and attributes. |
| Raw attributes | `include/houio/Attribute.h`, `src/Attribute.cpp` | Store fixed-width numeric tuple data in a byte buffer. |
| Dense fields | `include/houio/Field.h`, `src/Field.cpp` | Store and sample dense 3D scalar or vector grids. |
| Math | `include/houio/math/`, `src/math/` | Supply vectors, matrices, bounds, colors, rays, RNG, and half floats. |
| Vendored templates | `include/ttl/` | Provide the variant implementation used by the JSON layer. |
| Build and package | `CMakeLists.txt`, `CMakePresets.json`, `cmake/` | Define the C++20 target, presets, install tree, package version, and exported `houio::houio` target. |
| Converter tool | `tools/houio_convert.cpp` | Provide an installed path-to-path `.geo`/`.bgeo`/`.bgeo.sc` converter using the public API. |
| HOM bridge | `python/houio_hom/`, `tools/houdini/install_hom_bridge.ps1` | Integrate with Houdini 20.0+, preserve Float SDF/Fog semantics across VDB/dense-volume conversion, and invoke `houio_convert` with a bounded timeout. |
| Tests and examples | `tests/`, `tools/houdini/` | Provide modern-schema, SCF, HOM, package-consumer, and supported-Houdini regression coverage. |
| Legacy scene exporter | `scene_exporter/` | Export custom scene JSON from old Python 2 Houdini sessions. |

## 1. Binary JSON layer

### Core types

The binary JSON subsystem lives in the `houio::json` namespace.

Key types:

- `Parser`
- `Handler`
- `Token`
- `JSONReader`
- `JSONLogger`
- `Writer`
- `BinaryWriter`
- `ASCIIWriter`
- `Value`, `Array`, and `Object`

### Parser design

`Parser` is a state-machine parser. It reads from a caller-owned `std::istream` and emits events to a caller-provided `Handler`.

The parser handles:

- ASCII JSON tokens
- SideFX binary JSON identifiers
- Compact integer lengths
- String definitions, references, and undefinitions
- Uniform numeric arrays, including modern signed-int8 polygon run-length data
- Arrays and maps

The parser determines whether the input is binary by inspecting the stream's opening data. Every fixed-size read validates the exact byte count. Binary lengths reject negative values before conversion to container sizes, and string-token references must resolve to an existing definition. State transitions reject unmatched or mismatched closing tokens, missing map values, misplaced separators, and invalid top-level tokens. ASCII scalar roots are supported even when their final byte is the end of a seekable or streaming input.

`ParserLimits` provides per-parser bounds for total input bytes, string bytes, uniform-array elements, and nesting depth. The defaults are 1 GiB per document, 64 MiB per string, 64 million uniform-array elements, and 1,024 nested containers. Seekable streams are checked before parsing, while non-seekable streams enforce the same budget as bytes are consumed. Uniform-array payload sizes are validated before handler allocation, trailing data is rejected, and duplicate native map keys fail explicitly. `HouGeoIO::import()` uses these defaults, while its limits overload exposes the same controls to applications.

### Diagnostics

`include/houio/Diagnostic.h` defines the shared diagnostic boundary for parsing, semantic loading, and convenience conversion. Each record contains severity, category, message, optional byte position represented by `-1` when unavailable, and an optional schema path.

Parser reads maintain a monotonic byte offset. Truncated fixed-size values report the first missing byte, token failures report the token or storage-type byte, and parser-generated diagnostics distinguish malformed encodings from recognized-but-unsupported encodings. The diagnostics-aware parser overload catches these failures and returns `false`; the historical overload continues to throw.

`tests/parser_corpus.cpp` is shared by a deterministic CTest executable and an optional Clang/libFuzzer target. The deterministic pass exercises valid ASCII and binary seeds plus truncation, byte replacement, insertion, deletion, and reproducible randomized mutation through both `JSONReader` and `HouGeoIO::import()`. CI runs the corpus under normal compilers, GCC UndefinedBehaviorSanitizer, MSVC AddressSanitizer, and a bounded libFuzzer smoke session.

`HouGeo::load()` wraps attribute, topology, primitive, and group boundaries with schema paths. Unknown primitive types and unsupported group encodings preserve the `unsupported_input` category while gaining paths such as `primitives[0].definition.type`. Other semantic exceptions are promoted to `schema` diagnostics.

`HouGeoIO` exposes diagnostics-aware overloads for low-level import, simplified geometry import, volume import, and conversion. The caller owns the `DiagnosticList`. These overloads return null on error and never write error messages to standard output.

### Event handlers

`Handler` is the SAX-style boundary. It receives events such as:

```text
jsonBeginArray
jsonEndArray
jsonBeginMap
jsonEndMap
jsonString
jsonKey
jsonBool
jsonInt32
jsonReal32
uniform-array callbacks
```

Two handlers are central:

#### `JSONReader`

Builds an in-memory tree from `Value`, `Array`, and `Object`. Uniform arrays may retain packed storage to reduce per-element overhead. Their source and destination allocation sizes are checked before allocation, and temporary source storage is RAII-owned.

#### `JSONLogger`

Prints a readable representation of the parsed stream. This is an important reverse-engineering tool because the Houdini geometry schema is only partially encoded in the codebase and changes between Houdini versions.

### Writers

`BinaryWriter` writes SideFX binary JSON. `ASCIIWriter` exists, but the high-level geometry exporter depends on binary-only uniform-array methods. The stream export API therefore returns `false` for `binary=false` without writing partial output.

## 2. Flattened Houdini objects

Houdini geometry often represents an object as an ordered array of alternating keys and values instead of a normal JSON map:

```json
[
    "pointcount", 8,
    "vertexcount", 24,
    "primitivecount", 6
]
```

The ordering can carry schema significance. HouIO commonly converts these arrays into maps for convenient lookup using:

```cpp
HouGeo::toObject(array)
```

This conversion improves access but discards duplicate keys and original ordering. Code that depends on either characteristic must operate on the source `Array` instead.

## 3. Houdini adapter contracts

`HouGeoAdapter` is the abstract boundary between file-format writing and a client's geometry representation.

It defines access to:

- Point, vertex, primitive, and global attributes
- Topology
- Primitive collections
- Geometry counts

Nested interfaces describe the required data:

### `AttributeAdapter`

Exposes:

- Name
- Semantic type: numeric or string
- Tuple size
- Storage type
- Element count
- Raw numeric memory or indexed strings

### `Topology`

Exposes Houdini's vertex-to-point index list.

### `Primitive`

Base interface for supported primitive types.

Current derived contracts:

- `PolyPrimitive`
- `VolumePrimitive`

A client can implement these interfaces and export its own data without copying it into HouIO's `Geometry` class.

## 4. Houdini geometry model

`HouGeo` is the repository's concrete implementation of `HouGeoAdapter`.

It owns:

```text
m_pointAttributes
m_vertexAttributes
m_primitiveAttributes
m_globalAttributes
m_pointGroups
m_vertexGroups
m_primitiveGroups
m_topology
m_primitives
```

### Attribute domains

Each Houdini attribute is represented by `HouGeo::HouAttribute`, which wraps the generic `Attribute` buffer for numeric values or expanded per-element strings. Numeric loading supports legacy paged data, modern `values.tuples`, and component-oriented `values.arrays`. Paged loading validates page sizes, packing coverage, per-pack constant flags, partial final pages, and exact payload consumption before decoding. Signed 32-bit and 64-bit integers plus 16-bit, 32-bit, and 64-bit floating-point values retain their declared storage through import and export. String loading expands indexed string tables, including constant-page-compressed index payloads used by Houdini 21/22.

The loader uses the root geometry counts to size each domain:

- Point attributes: `pointcount`
- Vertex attributes: `vertexcount`
- Primitive attributes: `primitivecount`
- Global attributes: one element

### Topology

`HouTopology` stores the point-reference index buffer. Houdini primitive records may reference entries in this topology rather than point indices directly.

### Groups

Point, vertex, and primitive groups are stored as maps from names to boolean membership masks. The loader currently accepts Houdini's unordered `i8` selection encoding, validates each mask against its declared domain count, and rejects duplicate names, non-binary values, and ordered selections. The adapter exposes default no-op group accessors so existing custom exporters remain source compatible.

### Polygon primitives

`HouPoly` can represent one polygon or a run of polygons. It stores:

- Number of polygons
- Per-polygon vertex counts
- Per-polygon offsets
- Flattened point-index data
- Closed state

The loader recognizes direct `Poly` records, legacy `run` records whose `runtype` is `Poly`, Houdini 21/22 `Polygon_run` records, and open `PolygonCurve_run` records. Binary Houdini files may compact these names to `p_r` and `c_r`, with fields such as `s_v`, `n_p`, `r_v`, and `n_v`; the semantic loader accepts both spellings. Polygon runs support either run-length encoded counts or direct per-primitive vertex counts while validating the topology range and final primitive count.

### Volume primitives

`HouVolume` wraps a `ScalarField` and stores the topology vertex used to derive translation.

The legacy volume loader understands:

- 16×16×16 tiles, including partial boundary tiles
- Raw and raw-full tiles
- Constant tiles
- Constant arrays
- Shared voxel payloads

Volume loading validates positive three-dimensional resolution, checked voxel and tile products, exact tile counts and payload lengths, topology vertex references, the point `P` lookup used for translation, and the nine-component transform. Unsupported compression modes produce `unsupported_input` diagnostics with per-tile schema paths. `Field::resize()` also rejects negative or overflowing resolutions and handles empty storage without dereferencing an empty vector.

This is a dense volume path. It is not an OpenVDB implementation.

## 5. I/O facade

`GeometryIO` is the preferred path-based facade. `HouGeoIO` remains the lower-level stream boundary and compatibility layer.

### Path import flow

```text
GeometryIO::readHouGeo(path, options)
    ├─ Read with maxFileBytes bound
    ├─ Detect format from magic, then extension
    ├─ Validate and decompress SCF when present
    ├─ HouGeoIO::import(memory stream, ParserLimits)
    ├─ JSONReader builds a value tree
    └─ HouGeo::load interprets the Houdini schema
```

The returned `GeometryReadResult<T>` owns the value, diagnostics, and explicit success flag. Parsed objects do not refer to the source file, decompression buffer, or generic JSON tree after the call returns. The original `HouGeoIO::import(std::istream*)` contract remains synchronous and caller-owned: the stream must remain valid only for the duration of the call.

### Simplified geometry flow

```text
GeometryIO::readGeometry(path)
    ├─ Read HouGeo
    ├─ Select the first stored primitive representation
    └─ Convert supported polygon data into Geometry
```

This path is intentionally limited and lossy. It requires one `P` tuple per declared point, one fixed polygon size, domain-consistent point and vertex attributes, valid polygon ranges, and point references inside `pointcount`. Position and UV conversion use each attribute's declared tuple size as the stride, accepting three- or four-component `P` and two-or-more-component UV data without reading padding that may not exist. Vertex attributes are flattened into the point domain by duplicating points at discontinuities. Primitive/global attributes, groups, mixed polygon sizes, arbitrary n-gons, and unsupported numeric storage are not preserved.

### Volume flow

```text
GeometryIO::readVolumes(path)
    ├─ Read HouGeo
    ├─ Require every primitive to be a supported dense scalar volume
    └─ Return all ScalarField objects

GeometryIO::readVolume(path)
    ├─ Delegate to readVolumes
    ├─ Return the first field
    └─ Emit a conversion warning when additional fields are dropped
```

### Export flow

```text
GeometryIO::writeHouGeo(...) / writeGeometry(...) / writeVolume(...)
    ├─ Optionally adapt Geometry or Field<T> into HouGeo
    ├─ Serialize counts and topology
    ├─ Serialize point, vertex, primitive, and global attribute domains
    ├─ Serialize polygon-run or volume primitives
    ├─ Serialize point, vertex, and primitive group masks
    ├─ Finish the binary JSON root array
    └─ Optionally compress binary output into SCF blocks
```

The writer serializes point, vertex, primitive, and global attributes through the same adapter contract. Numeric values are emitted as paged uniform arrays. Per-element strings are deduplicated into a string table plus integer indices. Closed polygons are emitted as `Polygon_run`; open polygons use `PolygonCurve_run`. Each record stores a topology vertex offset plus direct per-primitive vertex counts, avoiding the historical ambiguity between topology offsets and point numbers. Unordered groups are emitted as named signed-int8 membership masks after validating their domain sizes. The writer promotes a three-component floating-point `P` attribute to four components with `w = 1`.

Each export owns its `BinaryWriter` and `ExportContext` on the stack, and every serialization helper receives that context explicitly. Adapter raw pointers are consumed synchronously and must remain valid until the call returns; HouIO does not retain them. There is no process-global or thread-local writer state, so independent files or streams can be exported concurrently. Historical `HouGeoIO` path APIs and `xport()` delegate to `GeometryIO`, while stream overloads remain available.

### SCF compression boundary

`src/Scf.cpp` implements SideFX's Seekable Compressed Format as a container around ordinary binary bgeo bytes. It validates:

- `scf1` header and `1fcs` footer
- Big-endian metadata and index lengths
- Sequential C-Blosc block headers
- Nominal and final uncompressed block sizes
- Cumulative compressed offsets in the seek index
- Configured compressed and decompressed byte limits

C-Blosc is resolved at runtime rather than linked into `houio::houio`. This keeps raw `.geo`/`.bgeo` users dependency-free and lets Houdini sessions use the Blosc version shipped with their active `$HFS`. Invalid wrappers fail before the JSON parser sees any decompressed bytes.

### HOM and OpenVDB boundary

The standalone C++ library deliberately does not load Houdini's private `openvdb_sesi` binary or duplicate OpenVDB's sparse tree model. `GeometryFileFormat::openvdb` exists for detection and actionable diagnostics, not native sparse-grid parsing.

`python/houio_hom` is the supported Houdini-side adapter. It uses `hou.Geometry.loadFromFile()` and `saveToFile()` for Houdini-supported containers, `hou.Geometry.data()` and `load()` for in-memory bgeo bytes, and the `convertvdb` SOP verb for 32-bit Float VDB/dense-volume conversion. The bridge accepts `level set` and `fog volume` VDB classes, maps them to iso and smoke dense volumes, carries the authoritative class in the `houio_vdb_class` primitive attribute, and restores the VDB intrinsic on output. The C++ volume model also preserves Houdini visualization mode, iso value, and density instead of hardcoding smoke metadata. Float VDBs can be densified while preserving unrelated Houdini primitives; `.vdb` output remains restricted to pure dense-volume or VDB sets because Houdini's writer omits unrelated mesh primitives. `houio_convert` runs as a subprocess with a configurable 300-second default timeout, avoiding a CPython extension ABI tied to one Houdini Python version. Sparse VDBs are densified only by explicit caller choice and are therefore subject to potentially large memory amplification.

### Fixture-backed compatibility tests

The optional Houdini integration layer generates minimal fixtures rather than storing version-specific binary blobs in the repository. A manifest records counts, domains, primitive state, and known losses. HouIO round-trips each fixture, and Houdini 21.0.631 and 22.0.368 compare exact attribute data types, numeric precision and values, primitive topology, open/closed state, dense-volume resolution, transform, position and voxel values, and point, vertex, and primitive group membership. The 14-fixture matrix succeeds with either Houdini 21 or Houdini 22 as the generator and has no intentional round-trip losses.

The Crag integration test remains the large-scale gate. It additionally compares all 89,942 polygon topologies and 359,794 vertices exactly, which protects the topology-offset writer path from shared-point corruption.

## 6. Simplified `Geometry`

`Geometry` is a convenience mesh container intended for rendering-oriented use.

It stores:

- One primitive type for the entire object
- A flat index buffer
- A map of named `Attribute` objects
- Primitive counts and fixed vertex count per primitive

Supported primitive modes:

- Point
- Line
- Triangle
- Quad
- Polygon

It also provides geometry creators and operations such as grids, spheres, boxes, transforms, normal generation, merging, and point duplication.

### Vertex-to-point conversion

Houdini distinguishes point attributes from vertex attributes. `Geometry` does not preserve this distinction.

During conversion, HouIO:

1. Compares vertex attribute values associated with each shared point.
2. Marks points whose vertex values differ.
3. Duplicates those points.
4. Copies vertex values into point attributes on the resulting points.

This allows UV seams and similar discontinuities to render correctly, but it changes topology and loses the original domain semantics.

## 7. `Attribute` storage

`Attribute` stores fixed-width tuple data inside:

```cpp
std::vector<unsigned char> m_data;
```

Metadata records:

- Component count
- Component type
- Component byte size
- Element count

Append operations copy trivially stored tuple bytes with `std::memcpy`, validate component and element counts, and reject indexed raw-pointer access outside the stored range. The historical typed `get()` and `set()` methods still reinterpret the byte buffer as caller-selected types.

This design is compact and simple but still has architectural risks:

- Alignment assumptions in typed accessors
- Strict-aliasing concerns in typed accessors
- Limited runtime type validation
- Easy tuple-size mismatches
- Raw pointer lifetime coupling

Modernization should replace the remaining reinterpret-cast access with validated typed views or copy-based access while preserving the compact adapter representation.

## 8. Dense `Field<T>`

`Field<T>` is a templated dense grid container. It owns:

- Resolution
- Dense voxel vector
- Local-to-world and inverse transforms
- Sampling and interpolation operations

The implementation supports coordinate conversion between world, local, and voxel space. It is used by the legacy Houdini volume path and can also be used independently.

Because the full template implementation is in the header, `Field.h` is one of the largest and most important public files.

## 9. Math and vendored dependencies

HouIO is mostly self-contained. It includes custom vector, matrix, bounds, half-float, and utility types under `houio::math`.

It also vendors `ttl`, which supplies the variant implementation used by JSON values.

Advantages:

- Minimal external dependencies
- Easy static embedding
- Stable internal type vocabulary

Costs:

- More code to audit and maintain
- Duplicate functionality available in modern C++ and established libraries
- Potential undefined behavior or numerical bugs in lightly tested utilities

## 10. Legacy and disconnected components

### `scene_exporter/`

This directory contains a separate Houdini Python scene exporter. It exports cameras, transforms, lights, locators, channels, and other scene data to a custom `.scn` representation.

It uses Python 2 syntax and old Houdini APIs. It is not included in the C++ library target and should not be confused with `.geo` or `.bgeo` support.

### `HouScene`

`HouScene` is effectively an empty stub with its original implementation commented out.

### `ImportHoudini`

`ImportHoudini` is entirely commented out and references an external graph-node framework that is not present in this repository.

These components should either be removed, archived, or explicitly revived as separate projects.

## Architectural constraints

The current implementation assumes:

1. The complete JSON document can generally be represented in memory.
2. Geometry schema interpretation happens after generic parsing.
3. Polygon conversion targets a single primitive mode per `Geometry` object.
4. Client numeric attributes can be exposed through contiguous raw memory.
5. Legacy dense volumes are acceptable in memory.
6. Schema support can be extended through explicit conditionals for known record types.
7. SCF compression can buffer one complete uncompressed bgeo payload before file output.
8. HOM-assisted VDB conversion may densify sparse grids; HouIO accepts only 32-bit Float grids, and `.vdb` output requires a pure grid set.

These assumptions are reasonable for small legacy files but become problematic for large modern production geometry.

## Primary architectural risks

### Format evolution

The parser and schema model originated from reverse-engineered Houdini 13-era data, but the active compatibility floor is Houdini 20.0. Houdini 13 files are not supported. New record types or encodings outside the tested Houdini 20.0+ paths may fail, be ignored, or be interpreted incorrectly.

### Full-document memory use

`JSONReader` builds a generic document tree before `HouGeo` builds a second semantic representation. Large files can therefore consume substantially more memory than their final geometry representation.

### Explicit export context

`HouGeoIO` owns each writer on the stack and passes an explicit `ExportContext` through topology, attribute, primitive, and group serializers. This removes hidden writer state while preserving exception safety and parallel exports to independent streams.

### Split error models

`GeometryIO` returns owned results and structured diagnostics, but lower-level legacy APIs still mix null returns, `bool`, `DiagnosticException`, and generic exceptions. New path-based features should use result objects; compatibility overloads should remain thin delegates rather than introduce additional error conventions.

### Data model mismatch

`Geometry` cannot faithfully represent all Houdini domains and primitive mixtures. It should remain an explicit convenience conversion rather than become the canonical model. `ScalarField` still cannot represent sparse OpenVDB topology, inactive values, or arbitrary grid metadata; SDF/Fog class is preserved only by the Houdini bridge's primitive metadata and visualization mapping.

### Optional runtime dependencies

SCF requires a compatible C-Blosc shared library. The loader reports a clear unsupported-input diagnostic when none can be resolved, but deployment must provide that runtime explicitly outside Houdini. The HOM bridge additionally requires Houdini and should not become a hidden dependency of the standalone library.

### Undefined-behavior exposure

Raw byte buffers, casts, pointer arithmetic, and unchecked indexing require a dedicated safety pass and sanitizer coverage.

## Recommended architectural direction

Near-term work should preserve the current layers while improving their contracts:

1. Keep the binary JSON parser independent of Houdini geometry semantics.
2. Make `HouGeoAdapter` the stable export boundary.
3. Keep `HouGeo` as the lossless-as-supported Houdini model.
4. Keep `Geometry` as an explicitly lossy convenience representation.
5. Keep serialization helpers explicit and free of hidden writer state.
6. Keep `GeometryIO` as the stable result-based path facade while preserving thin legacy wrappers.
7. Keep SCF as an optional stream/container layer rather than mixing compression into JSON semantics.
8. Keep native OpenVDB optional; prefer an external OpenVDB adapter or the explicit HOM bridge over sparse-to-dense behavior in the core parser.
9. Add fixture-driven compatibility tests before supporting additional primitives.
10. Consider streaming semantic readers only after behavior is covered by tests.

A large rewrite before fixture coverage would be high risk because the existing code contains undocumented format knowledge.
