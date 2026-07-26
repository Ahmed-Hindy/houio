# Fixture generation and validation

HouIO fixtures are generated into the build tree with Houdini rather than committed as opaque binary files. The generator records deterministic counts, attribute names, group names, primitive closure, packed embedded-payload metadata, native VDB sparse summaries, the producing Houdini build, and any intentional losses in `manifest.json`.

## Maintained Houdini builds

The default Windows validation matrix is:

- 20.0.653
- 20.5.410
- 21.0.631
- 22.0.368

The generator defaults to 22.0.368. Each output is then loaded and compared independently by all four maintained versions.

Expected installation layout:

```text
C:\Program Files\Side Effects Software\Houdini <version>\bin\hython.exe
```

## Run the generated fixture matrix

From a Visual Studio 2022 development machine:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\houdini\run_fixture_roundtrips.ps1
```

The script performs these steps:

1. Configures `windows-msvc-release` with the generator's `hython.exe`.
2. Builds HouIO and its test executables.
3. Generates the source fixtures and manifest.
4. Round-trips every fixture through HouIO.
5. Runs the CTest fixture and SCF checks.
6. Loads source and candidate files in every requested Houdini version.
7. Compares counts, attributes, topology, groups, and supported volume data.

The default generator is Houdini 20.0.653, the oldest maintained build. This matters for native VDB fixtures because newer Houdini/OpenVDB payload versions may not be readable by older Houdini builds.

Override the matrix explicitly when the validation set does not include an older VDB reader:

```powershell
.\tools\houdini\run_fixture_roundtrips.ps1 `
  -GeneratorVersion 22.0.368 `
  -ValidationVersions 22.0.368
```

Generated files are written below:

```text
build/windows-msvc-release/tests/fixtures/source
build/windows-msvc-release/tests/fixtures/output
```

Do not commit these generated directories.

## Manifest contract

Each fixture entry contains:

- `name`: stable fixture identifier.
- `file`: generated source filename.
- `source`: deterministic source summary.
- `known_losses`: explicitly accepted differences.

The current validator permits only these loss keys:

- `point_groups`
- `vertex_groups`
- `primitive_groups`

Unknown keys fail validation so misspellings cannot silently weaken the matrix. Every maintained fixture currently declares no known losses.

When a new feature cannot round-trip exactly, first decide whether it should be implemented or rejected. Add a known loss only when the omission is intentional, documented in the compatibility matrix, and verified to produce no unexpected residual data.

## Adding a fixture

1. Add a deterministic builder to `tools/houdini/generate_fixture_suite.py`.
2. Keep the geometry minimal while exercising one format behavior.
3. Add the builder to the `fixtures` tuple with an explicit `known_losses` tuple.
4. Ensure values, topology, groups, and transforms are deterministic.
5. Run the complete four-version matrix.
6. Add malformed C++ tests when the feature introduces counts, ranges, references, or allocation boundaries.
7. Update `docs/compatibility.md` when the support boundary changes.

Fixture generation must not depend on the current scene, current time, user preferences, or files outside the repository and build directory.

## Run the large Crag round trip

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\houdini\run_crag_roundtrip.ps1
```

The default command generates the asset with Houdini 22.0.368, round-trips it through HouIO, and validates it with all four maintained versions. It compares exact topology and selected point, vertex, and primitive attributes.

Output is written below `build/crag`.

## Validate the Houdini package

Run the complete maintained package matrix:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\houdini\run_package_matrix.ps1
```

The script builds the archive and validates it with Houdini 20.0.653, 20.5.410, 21.0.631, and 22.0.368. Supply `-PackageArchive` to test an existing archive or `-ValidationVersions` to test another installed build.

For one Houdini build, the underlying command is:

```powershell
cmake `
  -DHOUIO_HOUDINI_PACKAGE_ARCHIVE=<archive.zip> `
  -DHOUIO_HOUDINI_PACKAGE_EXTRACT_DIR=<temporary-directory> `
  -DHOUIO_HYTHON_EXECUTABLE=<hython.exe> `
  -DHOUIO_HOUDINI_PACKAGE_TEST_SCRIPT=<repository>\tools\houdini\test_houdini_package.py `
  -P tests\run_houdini_package_test.cmake
```

The test extracts the package into an isolated directory, exposes it through a process-local package path, starts `hython`, and verifies package discovery and runtime dependencies without modifying the user's Houdini preferences.

## Interpreting a failure

- Generator failure: the selected Houdini build cannot create the requested source fixture.
- Manifest failure: source generation changed unexpectedly.
- C++ round-trip failure: HouIO rejected or changed the source file.
- Houdini load failure: the candidate is unreadable by that Houdini version.
- Semantic mismatch: the candidate loaded but changed compared data.
- Validator API failure: the validation script used a HOM method unavailable in that Houdini version; add a version-compatible fallback without weakening comparisons that the version can perform.
