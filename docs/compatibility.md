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

The generated suite contains 14 deterministic fixtures and compares the source and HouIO output inside each supported Houdini version. It validates:

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
- Point, vertex, primitive, and global attributes.
- Unordered point, vertex, and primitive groups, including primitive groups spanning mixed polygon and dense-volume records.
- Indexed string and dictionary data used by the covered fixtures.
- Attribute definition scope and complete semantic `options` objects.

SCF outer compression is supported through a compatible C-Blosc runtime.

## Recognized but unsupported records

The standalone C++ model does not currently preserve these records:

- Packed geometry, packed fragments, and packed disk primitives.
- NURBS and Bezier curves.
- Spheres, tubes, tetrahedra, and height fields.
- Agents, crowds, and instancing records.
- Native sparse OpenVDB trees.
- Vector VDB grids.
- Additional volume tile-compression encodings not represented by maintained fixtures.

Unsupported recognized input should produce an `unsupported_input` diagnostic rather than silent data loss.

## Lossless and simplified representations

Use `HouGeo` (`HoudiniGeometry`) or `HouGeoAdapter` when Houdini domain fidelity matters. The simplified `Geometry` (`SimplifiedMesh`) type is render-oriented and has different guarantees. The aliases are provided by `<houio/GeometryModels.h>` and do not replace the established class names:

- Vertex-domain discontinuities may require point duplication.
- Point identity can therefore change even when rendered attribute values are preserved.
- Mixed primitive families are not silently collapsed into one mesh.
- One arbitrary n-gon is supported; multiple variable-size polygons are not represented by a single `SimplifiedMesh`.
- Open polygons with three or more vertices become closed simplified faces; `GeometryConversionReport::polygonClosureLost` and a conversion warning expose that loss.
- Native sparse volumes and unsupported primitive records are not converted implicitly.

`HouGeoIO::convertToGeometryResult` reports source/output point counts, distinct split source points, duplicated points, winding reversal, skipped point/vertex/primitive/global attributes, dropped groups, and structured diagnostics for unsupported or invalid data. Callers that require faithful round trips should still stay on the Houdini-oriented model rather than treating a clean simplified conversion as proof that every domain was preserved.

## VDB bridge scope

The standalone library does not link to OpenVDB or preserve sparse trees. The Houdini Python bridge can explicitly convert supported Float SDF and Fog grids to dense scalar volumes, process them through HouIO, and restore the VDB class on output. This is a bridge workflow, not native sparse-grid support in the C++ data model.

## Distribution status

Technical compatibility does not make the repository ready for redistribution. A project-wide license and third-party provenance notices are still required before publishing source or binary releases.
