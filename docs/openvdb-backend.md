# Optional OpenVDB backend

HouIO keeps its default standalone library independent of OpenVDB. The public sparse-grid model is always available, while native `.vdb` file access is enabled only when HouIO is configured with an OpenVDB SDK.

## Configure

```powershell
cmake -S . -B build/openvdb -DHOUIO_ENABLE_OPENVDB=ON
cmake --build build/openvdb --config Release
```

`HOUIO_ENABLE_OPENVDB=ON` requires `find_package(OpenVDB)` to provide the imported target `OpenVDB::openvdb`. Current upstream OpenVDB installations commonly provide `FindOpenVDB.cmake`; add that installed module directory to `CMAKE_MODULE_PATH`. Config-package installations can instead be supplied through `OpenVDB_DIR` or `CMAKE_PREFIX_PATH`. The dependency is exported through the installed HouIO CMake package, so downstream consumers of an OpenVDB-enabled static build resolve the same dependency explicitly.

The default is `OFF`. A normal HouIO build therefore has no OpenVDB headers, libraries, or runtime dependency.

## Dependency-neutral sparse grid

`houio::SparseFloatGrid` is available in every build. It preserves:

- A finite float background value.
- Active voxel coordinates and values.
- Grid name and class (`unknown`, `fog_volume`, or `level_set`).
- A linear 4×4 index-to-world transform.
- String metadata.
- Deterministic active-voxel traversal and active index bounds.

An active voxel may have the same numeric value as the background. Activity and value are stored independently.

## Native backend API

`houio::OpenVdbBackend::info()` reports whether the optional backend was compiled and, when available, the OpenVDB version.

`readFloatGrid()` and `writeFloatGrid()` provide native file I/O. `encodeFloatGrid()` and `decodeFloatGrid()` provide equivalent in-memory OpenVDB streams for Houdini payload construction:

- One OpenVDB `FloatGrid` at a time.
- Sparse active voxel values without dense conversion.
- Linear transforms.
- Fog-volume and level-set classes.
- Grid names and string metadata.

`NativeVdbPayload` validates and converts between one standard OpenVDB stream and Houdini's tiled `vdb` primitive payload. `HouSparseVdb` uses that codec during BGEO/SCF serialization when the optional backend is compiled.

When the backend is disabled, OpenVDB-dependent operations return an `unsupported_input` diagnostic instead of attempting a dense fallback. Opaque native payload pass-through remains available in every build.

## Current boundary

This backend does not yet represent active OpenVDB tiles. Reading a `FloatGrid` that contains an active tile returns an explicit unsupported diagnostic rather than expanding an unbounded tile into voxels. Nonlinear/frustum transforms are also rejected.

Live HOM extraction supports scalar Float VDBs only when exact active topology can be proven from `activeVoxelCount()`, the exclusive active bounding box, and sampled values. It rejects active background-valued voxels, inactive interior values, active tiles, half-float policy, local-space grids, and scan domains larger than 16,777,216 voxels rather than approximating them.

Remaining backend increments are:

1. Active tile representation.
2. Integer and vector grid types.
3. Additional typed metadata and serialization policies.
4. Exact extraction paths for activity patterns HOM cannot currently expose.
