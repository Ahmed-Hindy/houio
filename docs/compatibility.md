# Compatibility matrix

HouIO separates compatibility into three independently tested surfaces:

1. Standalone GEO/BGEO/SCF file round trips.
2. Houdini-side loading and semantic validation.
3. Houdini package discovery and runtime startup.

The current maintained Windows matrix is:

| Houdini build | Generated fixture suite | Large Crag round trip | Headless package test |
| --- | --- | --- | --- |
| 20.0.653 | Pass | Pass | Pass |
| 20.5.410 | Pass | Pass | Pass |
| 21.0.631 | Pass | Pass | Pass |
| 22.0.368 | Pass | Pass | Pass |

The minimum supported Houdini line is 20.0. Compatibility is established by the executable tests described in [Fixture generation and validation](fixtures.md), not only by package installation paths or version-string checks.

## What the fixture matrix validates

The generated suite contains 18 deterministic fixtures and compares the source and HouIO output inside each supported Houdini version. It validates:

- Empty and point-only geometry.
- Point, vertex, primitive, and global attribute domains.
- Signed Int32 and Int64 values.
- Float16, Float32, and Float64 values.
- Indexed strings, including empty values.
- Multi-page attributes and partial final pages.
- Triangles, quads, mixed polygon sizes, and n-gons.
- Open polygons and multiple polygon-run records.
- Vertex UV seams.
- Point, vertex, and primitive groups.
- Dense scalar-volume resolution, transform, position, and voxel values.
- Embedded `PackedGeometry` payloads, pivot, transform, viewport LOD, and packed flags.
- Named `PackedFragment` identity, local bounds, pivot, transform, viewport LOD, attributes, and groups.
- External `PackedDisk` filenames, expansion policy, transform metadata, attributes, groups, bounds, and referenced payload content.
- Native VDB active bounds, values, grid class, value type, transform, and visualization metadata.
- BGEO and SCF round trips.

Every current fixture has an empty `known_losses` list, so the maintained matrix requires exact agreement for every field the validator compares.

Houdini 20.0 does not expose `hou.Attrib.numericDataType()`. On 20.0, the validator still compares attribute owner, broad data type, tuple size, values, names, counts, topology, and groups, but cannot independently query the numeric storage-precision enum. Direct numeric-storage metadata validation is active on Houdini 20.5 and newer.

## Large-asset validation

The Crag test validates a representative polygon asset containing:

- 90,085 points.
- 359,794 vertices.
- 89,942 polygon primitives.
- Point attribute `P`.
- Vertex attributes `N` and `uv`.
- Primitive string attribute `name`.
- Primitive integer attribute `piece`.

Across all four maintained Houdini builds, the round trip requires:

- Exact point, vertex, and primitive counts.
- Exact topology.
- Exact string and integer attribute values.
- Zero maximum absolute difference for the tested floating-point attributes.

## Supported standalone records

The Houdini-oriented `HouGeo` model currently supports:

- `Poly`.
- `Polygon_run`.
- `PolygonCurve_run`.
- Dense scalar `Volume` records.
- Embedded `PackedGeometry` records and shared geometry payloads.
- Named `PackedFragment` records with fragment attribute/name, bounds, transform, and embedded geometry payloads.
- External `PackedDisk` references with authored filename, expansion frame/policy, transform, pivot, viewport LOD, and packed flags.
- Native `VDB` records through opaque serialized-payload preservation.
- Point, vertex, primitive, and global attributes.
- Unordered point, vertex, and primitive groups, including primitive groups spanning mixed polygon and dense-volume records.
- Indexed string and dictionary data used by the covered fixtures.
- Attribute definition scope and complete semantic `options` objects.

SCF outer compression is supported through a compatible C-Blosc runtime.

## Recognized but unsupported records

The standalone C++ model does not currently preserve these records:

- Packed disk sequences.
- NURBS and Bezier curves.
- Spheres, tubes, tetrahedra, and height fields.
- Agents, crowds, and instancing records.
- Constructed or edited OpenVDB sparse trees without an optional OpenVDB backend; existing native VDB payloads are preserved.
- Vector VDB construction and editing.
- Additional volume tile-compression encodings not represented by maintained fixtures.

Unsupported recognized input should produce an `unsupported_input` diagnostic rather than silent data loss. File-level `PackedFragment` records are supported, while direct HOM extraction remains unavailable because HOM does not expose the fragment's embedded source detail. `PackedDisk` references are supported in file and direct-HOM workflows; HouIO preserves the authored path and does not require the target to exist during serialization.

## Lossless and simplified representations

Use `HouGeo` (`HoudiniGeometry`) or `HouGeoAdapter` when Houdini domain fidelity matters. The simplified `Geometry` (`SimplifiedMesh`) type is render-oriented and has different guarantees. The aliases are provided by `<houio/GeometryModels.h>` and do not replace the established class names:

- Vertex-domain discontinuities may require point duplication.
- Point identity can therefore change even when rendered attribute values are preserved.
- Mixed primitive families are not silently collapsed into one mesh.
- One arbitrary n-gon is supported; multiple variable-size polygons are not represented by a single `SimplifiedMesh`.
- Open polygons with three or more vertices become closed simplified faces; `GeometryConversionReport::polygonClosureLost` and a conversion warning expose that loss.
- Native VDB payloads are retained by the Houdini-oriented model but are not converted into a simplified mesh.
- Unsupported primitive records are not converted implicitly.

`HouGeoIO::convertToGeometryResult` reports source/output point counts, distinct split source points, duplicated points, winding reversal, skipped point/vertex/primitive/global attributes, dropped groups, and structured diagnostics for unsupported or invalid data. Callers that require faithful round trips should still stay on the Houdini-oriented model rather than treating a clean simplified conversion as proof that every domain was preserved.

## VDB scope

The standalone library does not link to OpenVDB, but it recognizes native Houdini `VDB` records and preserves their serialized sparse payload opaquely during GEO/BGEO/SCF round trips. This retains active topology, values, transform, class, value type, and metadata without densification. It does not expose sparse-tree construction or editing.

The direct HOM manifest cannot currently create that native payload because HOM does not expose the serialized tree. The compatibility bridge can still convert supported Float SDF/Fog grids through dense volumes. A future optional OpenVDB backend is required for native live-session tree construction.

## Distribution status

Technical compatibility does not make the repository ready for redistribution. A project-wide license and third-party provenance notices are still required before publishing source or binary releases.
