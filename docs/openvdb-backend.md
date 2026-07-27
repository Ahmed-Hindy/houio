# Optional OpenVDB backend

HouIO keeps its default standalone library independent of OpenVDB. The public sparse-grid model is always available, while native `.vdb` file access is enabled only when HouIO is configured with an OpenVDB SDK.

## Configure

```powershell
cmake -S . -B build/openvdb -DHOUIO_ENABLE_OPENVDB=ON
cmake --build build/openvdb --config Release
```

`HOUIO_ENABLE_OPENVDB=ON` requires `find_package(OpenVDB)` to provide the imported target `OpenVDB::openvdb`. Current upstream OpenVDB installations commonly provide `FindOpenVDB.cmake`; add that installed module directory to `CMAKE_MODULE_PATH`. Config-package installations can instead be supplied through `OpenVDB_DIR` or `CMAKE_PREFIX_PATH`. The dependency is exported through the installed HouIO CMake package, so downstream consumers of an OpenVDB-enabled static build resolve the same dependency explicitly.

The default is `OFF`. A normal HouIO build therefore has no OpenVDB headers, libraries, or runtime dependency.

## Dependency-neutral sparse grids

`houio::SparseFloatGrid` and `houio::SparseInt32Grid` are available in every build. Both use the same typed sparse-scalar topology implementation and preserve:

- A typed background value.
- Active voxel coordinates and typed values.
- Active tiles as inclusive index-space bounds plus one typed value.
- Grid name and class (`unknown`, `fog_volume`, or `level_set`).
- A linear 4×4 index-to-world transform.
- String metadata.
- Deterministic active-voxel traversal and active index bounds.

Float values must be finite. Int32 values retain their exact signed 32-bit representation. An active voxel may equal the background numerically; activity and value are stored independently.

## Native backend API

`houio::OpenVdbBackend::info()` reports whether the optional backend was compiled and, when available, the OpenVDB version.

`readFloatGrid()` / `writeFloatGrid()` and `readInt32Grid()` / `writeInt32Grid()` provide native file I/O. Their `encode*Grid()` and `decode*Grid()` counterparts provide equivalent in-memory OpenVDB streams for Houdini payload construction:

- One OpenVDB `FloatGrid` or `Int32Grid` at a time.
- Sparse active voxel and active-tile values without dense conversion.
- Linear transforms.
- Grid class metadata, including fog-volume and level-set values where authored.
- Grid names and string metadata.

`NativeVdbPayload` validates and converts between one standard OpenVDB stream and Houdini's tiled `vdb` primitive payload. `encodeStream()` accepts a stream-producing callback and emits fixed-size tiles incrementally, so `HouSparseVdb` and `HouSparseInt32Vdb` construction do not retain a second complete OpenVDB byte buffer during BGEO/SCF serialization.

When the backend is disabled, OpenVDB-dependent operations return an `unsupported_input` diagnostic instead of attempting a dense fallback. Opaque native payload pass-through remains available in every build.

## Current boundary

The sparse models represent active OpenVDB tiles as inclusive index-space bounds plus one typed scalar value. Reads preserve active tiles without densifying them; writes reconstruct the active regions through OpenVDB tree filling and then apply explicit voxel overrides. Explicit `houio.hom/1` manifests can construct the same topology through an optional `active_tiles` array:

```json
"active_tiles": [
  {
    "minimum": [-8, -8, -8],
    "maximum": [-1, -1, -1],
    "value": 0.25
  }
]
```

Bounds are inclusive, must be ordered, and duplicate bound pairs are rejected. `active_indices` and `active_values` are optional, but must appear together when explicit voxels are present. Explicit active voxels take precedence over tile values. The same schema is available as `sparse_float_vdb` for FloatGrid values and `sparse_int32_vdb` for signed 32-bit integer values. Pure active tiles remain tiles through native OpenVDB round trips. When an explicit voxel overrides a tile inside the same OpenVDB leaf, OpenVDB necessarily refines that leaf; decoding may therefore return an equivalent set of explicit active voxels rather than the original non-canonical tile-plus-override decomposition. Activity and values remain exact. Nonlinear/frustum transforms are still rejected.

Live HOM extraction supports scalar Float VDBs only when exact active topology can be proven from `activeVoxelCount()`, the exclusive active bounding box, and sampled values. It rejects active background-valued voxels, inactive interior values, active tiles, half-float policy, local-space grids, and scan domains larger than 262,144 voxels rather than approximating them.

Remaining backend increments are:

1. Vec3f and other vector grid types with explicit vector semantics.
2. Additional typed metadata and serialization policies.
3. Nonlinear/frustum transform representation.
4. Exact extraction paths for activity patterns and value types HOM cannot currently expose.
