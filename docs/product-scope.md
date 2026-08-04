# Product scope

HouIO is a standalone Houdini geometry I/O SDK and toolchain. Its core responsibility is to move geometry between caller-owned data, Houdini GEO/BGEO containers, and selected scene archive formats without making the core library depend on the Houdini Development Kit.

## Shipped product surfaces

### `houio::houio`

The C++20 static library provides:

- GEO, BGEO, and BGEO.SCF reading and writing.
- Faithful Houdini-oriented `HouGeo` geometry.
- The smaller `SimplifiedMesh` convenience model.
- Structured diagnostics, parser limits, and safe path-based I/O.
- Dense fields and dependency-neutral sparse grid models.
- Optional OpenVDB, Alembic, and OpenUSD backends.

### `houio`

The unified command-line application provides:

- `write` and `write-manifest`.
- `convert`.
- `inspect` and `validate`.
- `capabilities` and `diagnose`.

The beta-only `houio_convert` executable is retired. File conversion is provided exclusively by `houio convert`.

### Houdini package

The Houdini package provides:

- Direct HOM extraction into the HouIO-owned manifest boundary.
- Shelf tools for writing selected geometry and package diagnostics.
- An optional native `/out` ROP that consumes the same HouIO writer implementation through a C ABI.

The supported Houdini range is 20.0 through 22.0 in the maintained validation matrix.

## Optional backends

| Backend | Adds | Core dependency |
| --- | --- | --- |
| OpenVDB | Standalone `.vdb` I/O and native VDB payload construction | Optional |
| Alembic | Polygon mesh and polyline archive writing | Optional |
| OpenUSD | Polygon mesh and polyline stage writing | Optional |
| Houdini HDK | Native ROP integration | Isolated from the C++20 core |

Alembic and USD are currently write-only geometry archive backends. HouIO is not a general scene graph, material, lighting, or camera authoring framework.

## Deliberate non-goals

HouIO does not currently attempt to provide:

- HIP-file or Houdini-node serialization.
- Complete Alembic or USD reading.
- Materials, shader networks, cameras, or lights.
- Every Houdini primitive family.
- A replacement for all SideFX native writers.
- Automatic legal redistribution rights for the upstream-derived combined work.

Unsupported or partially supported data must be rejected or reported rather than silently flattened.

## Build profiles

The named profiles keep product builds separate from the full development matrix:

| Profile | Command | Purpose |
| --- | --- | --- |
| Core minimal | `cmake --preset windows-msvc-core-minimal` | Library only; no tools, tests, scene writers, OpenVDB, or HDK |
| Core with OpenVDB | `cmake --preset linux-gcc-core-openvdb` | Core and CLI with the optional system OpenVDB backend |
| Scene I/O | `cmake --preset windows-msvc-scene-io` | Core and CLI linked to the pinned bundled Alembic/OpenUSD prefix |
| Houdini package | `.\tools\dev.ps1 package` | HOM package and private package archive |
| Full development | `cmake --preset windows-msvc-full-development` | Tools, examples, benchmarks, tests, and warnings-as-errors |

The scene-I/O profile requires `HOUIO_BUNDLED_SCENE_IO_ROOT`. Build the pinned dependency prefix first with:

```powershell
.\tools\dev.ps1 scene-deps -ConfirmLargeDownload
```

## Repository areas

| Area | Responsibility |
| --- | --- |
| `include/houio`, `src` | Product library and optional backends |
| `tools/houio.cpp` | Primary CLI |
| `python/houio_hom`, `houdini` | Houdini integration and package assets |
| `tests` | Unit, fixture, package, sanitizer, and integration validation |
| `tools/houdini`, `tools/dependencies` | Developer-only fixture, package, and dependency automation |
| `benchmarks` | Opt-in performance baselines |
| `docs`, roadmap files | Current contracts and historical engineering record |

The tests and automation are intentionally extensive because the supported binary format and Houdini-version matrix are the main compatibility risk. They are not separate runtime products.

## Generated data

The ignored `build/` directory can be much larger than the tracked repository because it may contain multiple compiler configurations, four Houdini-version test trees, generated fixtures, and compiled OpenUSD/Alembic dependencies.

Use the developer cleanup command instead of deleting individual directories manually:

```powershell
.\tools\dev.ps1 clean -Preset windows-msvc-release
.\tools\dev.ps1 clean -CleanScope builds -ConfirmClean
.\tools\dev.ps1 clean -CleanScope dependencies -ConfirmClean
.\tools\dev.ps1 clean -CleanScope all -ConfirmClean
```

The default `preset` scope removes only `build/<preset>`. The broader scopes require explicit confirmation; `builds` preserves the dependency cache.
