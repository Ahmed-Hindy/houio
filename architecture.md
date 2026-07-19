# Architecture

This document describes HouIO's current architecture as implemented in the repository. It separates the core file-format code from convenience data structures and abandoned experimental components.

## System overview

HouIO has four main layers:

```text
.geo / .bgeo
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
HouGeoIO export orchestration
      │
      ▼
BinaryWriter
      │
      ▼
.bgeo
```

## Module map

| Module | Primary files | Responsibility |
| --- | --- | --- |
| Binary JSON | `include/houio/json.h`, `src/json.cpp` | Tokenize, parse, log, and write Houdini ASCII and binary JSON. |
| Houdini adapter contracts | `include/houio/HouGeoAdapter.h`, `src/HouGeoAdapter.cpp` | Define the abstract geometry interface consumed by the writer. |
| Houdini geometry model | `include/houio/HouGeo.h`, `src/HouGeo.cpp` | Interpret Houdini's geometry schema and store attributes, topology, polygons, and volumes. |
| I/O facade | `include/houio/HouGeoIO.h`, `src/HouGeoIO.cpp` | Coordinate import, conversion, logging, and export. |
| Simplified geometry | `include/houio/Geometry.h`, `src/Geometry.cpp` | Store render-oriented points, indices, and attributes. |
| Raw attributes | `include/houio/Attribute.h`, `src/Attribute.cpp` | Store fixed-width numeric tuple data in a byte buffer. |
| Dense fields | `include/houio/Field.h`, `src/Field.cpp` | Store and sample dense 3D scalar or vector grids. |
| Math | `include/houio/math/`, `src/math/` | Supply vectors, matrices, bounds, colors, rays, RNG, and half floats. |
| Vendored templates | `include/ttl/` | Provide the variant implementation used by the JSON layer. |
| Tests and examples | `tests/` | Provide historical fixtures, binary-token and modern-schema regression tests, round-trip and inspection CLIs, and optional Houdini integration tests. |
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

The parser determines whether the input is binary by inspecting the stream's opening data.

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

Builds an in-memory tree from `Value`, `Array`, and `Object`. Uniform arrays may retain packed storage to reduce per-element overhead.

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
m_topology
m_primitives
```

### Attribute domains

Each Houdini attribute is represented by `HouGeo::HouAttribute`, which wraps the generic `Attribute` buffer for numeric values or a string vector for string attributes. Numeric loading supports the legacy paged representation and modern `values.tuples` arrays observed in Houdini 21/22.

The loader uses the root geometry counts to size each domain:

- Point attributes: `pointcount`
- Vertex attributes: `vertexcount`
- Primitive attributes: `primitivecount`
- Global attributes: one element

### Topology

`HouTopology` stores the point-reference index buffer. Houdini primitive records may reference entries in this topology rather than point indices directly.

### Polygon primitives

`HouPoly` can represent one polygon or a run of polygons. It stores:

- Number of polygons
- Per-polygon vertex counts
- Per-polygon offsets
- Flattened point-index data
- Closed state

The loader recognizes direct `Poly` records, legacy `run` records whose `runtype` is `Poly`, and Houdini 21/22 `Polygon_run` records. Binary Houdini files may compact these names to `p_r`, `s_v`, `n_p`, and `r_v`; the semantic loader accepts both spellings. `Polygon_run` expands run-length encoded vertex counts while validating the topology range and final primitive count.

### Volume primitives

`HouVolume` wraps a `ScalarField` and stores the topology vertex used to derive translation.

The legacy volume loader understands:

- 16×16×16 tiles
- Raw and raw-full tiles
- Constant tiles
- Constant arrays
- Shared voxel payloads

This is a dense volume path. It is not an OpenVDB implementation.

## 5. I/O facade

`HouGeoIO` is the main public entry point.

### Import flow

```text
HouGeoIO::import(stream)
    ├─ Parser parses stream
    ├─ JSONReader builds a value tree
    ├─ Root flattened array becomes an Object
    └─ HouGeo::load interprets the Houdini schema
```

### Simplified geometry flow

```text
HouGeoIO::importGeometry(path)
    ├─ Import HouGeo
    ├─ Select the first stored primitive representation
    └─ Convert supported polygon data into Geometry
```

This path is intentionally limited. It does not merge arbitrary primitive collections or preserve every Houdini semantic.

### Volume flow

```text
HouGeoIO::importVolume(path)
    ├─ Import HouGeo
    ├─ Select the first primitive
    └─ Return its ScalarField when it is a HouVolume
```

### Export flow

```text
HouGeoIO::xport(...)
    ├─ Optionally adapt Geometry or Field<T> into HouGeo
    ├─ Serialize counts and topology
    ├─ Serialize point, vertex, and primitive attribute domains
    ├─ Serialize polygon or volume primitives
    └─ Finish the binary JSON root array
```

The writer serializes point, vertex, and primitive attributes through the same adapter contract. It promotes a three-component floating-point `P` attribute to four components with `w = 1`, preserving compatibility with its legacy binary schema. Each export owns its `BinaryWriter` on the stack. A scoped thread-local binding lets the legacy helper functions reach the active writer while restoring the previous binding after normal completion or exceptions. Independent streams can therefore be exported concurrently.

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

Templated methods reinterpret the byte buffer as caller-selected types.

This design is compact and simple but has architectural risks:

- Alignment assumptions
- Strict-aliasing concerns
- Limited runtime type validation
- Easy tuple-size mismatches
- Raw pointer lifetime coupling

Modernization should either make these invariants explicit and validated or replace the storage abstraction.

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

These assumptions are reasonable for small legacy files but become problematic for large modern production geometry.

## Primary architectural risks

### Format evolution

The parser and schema model are based on reverse-engineered Houdini 13-era data. New record types or encodings may fail, be ignored, or be interpreted incorrectly.

### Full-document memory use

`JSONReader` builds a generic document tree before `HouGeo` builds a second semantic representation. Large files can therefore consume substantially more memory than their final geometry representation.

### Transitional writer binding

`HouGeoIO` no longer owns a process-global writer. The active stack-owned writer is exposed to legacy serialization helpers through a scoped thread-local pointer. This is exception-safe and permits parallel exports to independent streams, but passing an explicit export context through the helpers would remove the remaining implicit state.

### Weak error model

Many failures return null pointers, print to standard output, or throw generic `std::runtime_error` messages without file offsets or schema context.

### Data model mismatch

`Geometry` cannot faithfully represent all Houdini domains and primitive mixtures. It should remain an explicit convenience conversion rather than become the canonical model.

### Undefined-behavior exposure

Raw byte buffers, casts, pointer arithmetic, and unchecked indexing require a dedicated safety pass and sanitizer coverage.

## Recommended architectural direction

Near-term work should preserve the current layers while improving their contracts:

1. Keep the binary JSON parser independent of Houdini geometry semantics.
2. Make `HouGeoAdapter` the stable export boundary.
3. Keep `HouGeo` as the lossless-as-supported Houdini model.
4. Keep `Geometry` as an explicitly lossy convenience representation.
5. Replace the transitional thread-local writer binding with an explicit export context parameter.
6. Introduce structured diagnostics with byte offsets and schema paths.
7. Add fixture-driven compatibility tests before supporting additional primitives.
8. Consider streaming semantic readers only after behavior is covered by tests.

A large rewrite before fixture coverage would be high risk because the existing code contains undocumented format knowledge.
