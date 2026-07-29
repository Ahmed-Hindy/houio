# Alembic and USD writers

HouIO owns the Alembic and USD writer implementation. The native Houdini ROP is one consumer of this API; it is not the implementation boundary.

## Architecture

```text
HouGeoAdapter or simplified Geometry
  -> SceneGeometryAdapter
  -> SceneGeometrySample
  -> SceneArchiveWriter
  -> Alembic or OpenUSD backend
  -> .abc / .usd / .usda / .usdc
```

The HDK path uses the same writer through a C ABI:

```text
GU_Detail
  -> HoudiniGeometryAdapter
  -> HouIONativeSceneArchive C ABI
  -> HouIO SceneArchiveWriter
```

No Alembic, USD, STL, or C++20 object crosses the HDK compatibility boundary.

## Public API

For one static sample, use the primary writer facade:

```cpp
const houio::WriteResult result = houio::Writer::write(
    "cache/asset.usdc",
    geometry);
```

The destination extension selects `.abc`, `.usd`, `.usda`, or `.usdc`.

For animation, open one stateful archive and append samples:

```cpp
houio::SceneArchiveOptions options;
options.destination = "cache/asset.abc";
options.framesPerSecond = 24.0;
options.startFrame = 1.0;

auto writer = houio::createSceneArchiveWriter(options);
writer->writeSample(frame1, 1.0);
writer->writeSample(frame2, 2.0);
writer->finish();
```

The CLI exposes static conversion:

```powershell
houio convert input.bgeo output.abc
houio convert input.bgeo output.usda
houio convert input.bgeo output.usdc
```

## Dependency providers

Configure `HOUIO_SCENE_IO_PROVIDER` with one of:

| Provider | Purpose |
|---|---|
| `disabled` | Dependency-free development and CI build. Scene APIs remain present and return explicit unavailable-backend diagnostics. |
| `houdini` | Temporary integration validation against the matching Houdini SDK. This provider must not be used for HouIO release redistribution. |
| `system` | Build against externally installed upstream Alembic and OpenUSD CMake packages. |
| `bundled` | Release build against HouIO's pinned dependency prefix supplied through `HOUIO_BUNDLED_SCENE_IO_ROOT`. |

The bundled provider is the intended release configuration. It packages upstream Alembic/OpenUSD outputs and their notices; it does not copy or redistribute SideFX libraries.

## Bundled release build

Pinned versions are maintained in `cmake/scene-io-versions.cmake`. The Windows dependency builder downloads and compiles:

- OpenUSD 26.05;
- Alembic 1.8.12;
- Imath 3.2.2;
- oneTBB 2021.12.0 through OpenUSD's official builder.

Prerequisites are Visual Studio 2022 with the x64 C++ toolchain, CMake 3.29 or newer, Git, and `uv`. The large download and compile require explicit confirmation:

```powershell
.\tools\dev.ps1 scene-deps -ConfirmLargeDownload
```

After the dependency prefix exists, build, test, validate, and package HouIO with:

```powershell
.\tools\dev.ps1 scene-package
```

This produces:

```text
build/windows-msvc-release-bundled/houio-<version>-windows-x86_64.zip
build/windows-msvc-release-bundled/houio-<version>-windows-x86_64.zip.sha256
```

The ZIP is a portable SDK/runtime package containing:

- `houio.exe`, `houio_convert.exe`, and `houio.lib`;
- upstream runtime DLLs beside the executables;
- Alembic, Imath, OpenUSD, and oneTBB headers/import libraries;
- relocatable Alembic/OpenUSD CMake package metadata;
- OpenUSD plugin/resource registries;
- required third-party license and notice files;
- a dependency manifest with exact Git revisions and SHA-256 hashes for every packaged runtime DLL.

`tools/dependencies/validate_bundled_scene_io.ps1` removes Houdini, PXR, and Python environment variables, reduces `PATH` to the package plus Windows system directories, writes Alembic/USDA/USDC, verifies dependency hashes and notices, and rejects SideFX/Houdini binary imports.

## Current geometry scope

The first writer slice preserves:

- closed polygon meshes;
- open polygonal polylines;
- Float16, Float32, or Float64 point `P` adapted to Float32 archive positions;
- animated point samples;
- constant mesh and curve topology;
- frame rate and start/end sample timing;
- USD default prim, Y-up, and `metersPerUnit = 1.0`;
- create-directory, overwrite, and atomic-replacement policies.

To avoid silent loss, it currently rejects:

- public attributes other than point `P`;
- point, vertex, primitive, or detail groups/attributes;
- topology-changing animation;
- points-only geometry;
- true NURBS and Bezier curves;
- packed primitives, quadrics, volumes, and VDB;
- hierarchy and path-attribute reconstruction.

## Validation

The implementation is tested at four boundaries:

1. Dependency-neutral sample adaptation and failure behavior.
2. Standalone `Writer`, C ABI, and CLI output.
3. Native Houdini ROP animated round trips across maintained Houdini versions.
4. Extracted portable-package execution with no Houdini path, variables, or binary imports.

The bundled Windows configuration passes the complete 32-test suite, an installed downstream CMake consumer, archive extraction, third-party notice checks, 43 runtime DLL SHA-256 checks, and clean Alembic/USDA/USDC writes. Generated Alembic files are also checked with Alembic tooling; USD output is validated through the upstream runtime and Houdini integration tests.
