# TODO

This backlog prioritizes making HouIO buildable, testable, diagnosable, and verifiably compatible before expanding its feature set.

The priority labels indicate dependency order rather than estimated effort.

- **P0** — blocks responsible use or further development
- **P1** — required for a trustworthy maintained library
- **P2** — compatibility and feature expansion
- **P3** — cleanup or optional redesign

## P0 — Establish a legal and reproducible baseline

### Licensing

- [ ] Contact the original author and obtain an explicit license for the existing code.
- [ ] Add the approved license file without guessing or selecting a license on the author's behalf.
- [ ] Document whether vendored `ttl` and half-float sources have separate license obligations.
- [ ] Record provenance for all third-party code under `include/ttl/` and `include/houio/math/Half/`.

### Build system

- [x] Remove the duplicate `add_subdirectory(tests)` call from the root `CMakeLists.txt`.
- [x] Raise the minimum CMake version to a supported baseline.
- [x] Replace global `INCLUDE_DIRECTORIES` use with target-scoped includes.
- [x] Add proper `BUILD_INTERFACE` and `INSTALL_INTERFACE` include paths.
- [x] Define the namespaced CMake target `houio::houio`.
- [x] Verify a C++20 static build with Visual Studio 2022 and Ninja.
- [ ] Verify builds with Clang or GCC on Linux.
- [ ] Decide whether `houio.pro` remains supported or should be removed.
- [x] Verify install and package-config rules from a clean external consumer project.

### Test baseline

- [x] Call `enable_testing()` and register the historical logger with CTest.
- [ ] Replace print-only smoke behavior with assertion-based tests.
- [ ] Add tests for the supplied Houdini 13 ASCII fixtures.
- [ ] Add tests for the supplied Houdini 13 binary fixtures.
- [ ] Assert point, vertex, and primitive counts.
- [ ] Assert topology and representative attribute values.
- [ ] Assert legacy volume resolution, transform, and sample values.
- [ ] Add malformed and truncated input tests.
- [ ] Make test failures return non-zero process exit codes.

### Continuous integration

- [x] Add Windows MSVC CI.
- [ ] Add Linux Clang or GCC CI.
- [x] Add Debug and Release CMake presets.
- [x] Run CTest with failure output enabled.
- [x] Enable warnings without immediately treating legacy warnings as errors.
- [ ] Add a separate strict-warning job once the warning baseline is clean.

## P1 — Make current behavior safe and maintainable

### Houdini 21 and 22 compatibility baseline

- [x] Add a reproducible Houdini 21.0.631 and 22.0.368 Crag generator.
- [x] Freeze Crag in its rest T-pose with no time dependency.
- [x] Add optional CTest generation, round-trip, and Houdini validation stages.
- [x] Validate binary HouIO output in both Houdini 21.0.631 and 22.0.368.
- [ ] Generate minimal purpose-specific Houdini 21/22 `.geo` fixtures for individual schema features.
- [x] Generate and round-trip uncompressed Houdini 21/22 Crag `.bgeo` data.
- [x] Record the exact Houdini builds used by the Crag experiment.
- [ ] Test points with `P`, `Cd`, float, integer, and string attributes.
- [ ] Test point, vertex, primitive, and global attribute domains comprehensively.
- [ ] Test triangle-only polygon runs.
- [ ] Test quad-only polygon runs.
- [ ] Test vertex UV seams.
- [ ] Test empty geometry.
- [ ] Test dense scalar volumes if Houdini 21 still emits the legacy representation.
- [ ] Document unsupported Houdini 21 records rather than silently ignoring them.

### Structured diagnostics

- [ ] Introduce an error or diagnostic type with severity and message.
- [ ] Include stream byte offsets in parser errors.
- [ ] Include schema paths such as `attributes.pointattributes[2]` in semantic errors.
- [ ] Replace library `std::cout` messages with caller-controlled diagnostics.
- [ ] Distinguish malformed input from unsupported input.
- [ ] Ensure unsupported primitive types produce useful diagnostics.
- [ ] Preserve backward-compatible null-return behavior only where explicitly documented.

### Parser safety

- [x] Fix the fast-fail caused by unsupported uniform signed-int8 arrays in modern binary `Polygon_run` data.
- [ ] Check stream state after every fixed-size read.
- [ ] Validate binary lengths before allocation.
- [ ] Add configurable file-size, array-size, and nesting-depth limits.
- [ ] Detect integer overflow in byte-count calculations.
- [x] Add regression coverage for uniform signed-int8 widening.
- [ ] Validate all uniform-array element types and storage sizes before reading.
- [ ] Validate string token references before lookup.
- [ ] Reject odd-length flattened key/value arrays.
- [ ] Add bounds checks before all array and topology indexing.
- [ ] Add fuzz testing for the JSON parser.
- [x] Add and run an MSVC AddressSanitizer preset.
- [ ] Add UndefinedBehaviorSanitizer coverage on a supported compiler.

### Export safety

- [x] Remove unused `g_geo` and process-global writer ownership.
- [x] Use a scoped thread-local binding for the active per-export writer.
- [x] Restore the previous binding after normal completion or exceptions.
- [x] Make concurrent exports safe when streams are independent.
- [x] Use stack ownership for writers instead of manual `new` and `delete`.
- [x] Validate output stream state and propagate write failures.
- [ ] Pass an explicit export context through helper functions instead of a thread-local binding.
- [ ] Clarify ownership and lifetime requirements for raw pointers exposed by adapters.

### Correctness fixes

- [ ] Guard `importVolume()` against an empty primitive list.
- [ ] Initialize all `HouPoly` members deterministically.
- [ ] Load and preserve polygon closed state where present.
- [x] Verify `Polygon_run` vertex indexing against static Houdini 21/22 Crag geometry.
- [ ] Verify point-count behavior when no point attributes exist.
- [ ] Check every `dynamic_pointer_cast` result before dereferencing it.
- [ ] Avoid inserting null attributes into `Geometry`.
- [ ] Verify 16-bit topology export handles negative and out-of-range indices correctly.
- [ ] Verify matrix conventions, multiplication order, and volume transforms.

### Public API documentation

- [ ] Document ownership for every stream, pointer, and returned object.
- [ ] Document supported primitive and attribute types.
- [ ] Document lossy behavior in `convertToGeometry()`.
- [ ] Document exception and null-return behavior.
- [ ] Add small compilable examples for import, export, logging, and custom adapters.
- [ ] Decide whether to preserve the historical `xport` spelling or add a compatible `exportGeometry` API.

## P1 — Improve the test architecture

### Fixture organization

- [ ] Separate fixtures by Houdini version.
- [ ] Separate ASCII, binary, malformed, and unsupported fixtures.
- [ ] Add a manifest describing how each fixture was generated.
- [ ] Keep fixtures minimal and purpose-specific.
- [ ] Avoid large production assets in the repository.

Suggested structure:

```text
tests/fixtures/
    houdini-13.0.288/
        ascii/
        binary/
    houdini-21.0.x/
        ascii/
        binary/
    malformed/
    unsupported/
```

### Test categories

- [ ] Unit-test binary integer length encodings.
- [ ] Unit-test every supported token type.
- [ ] Unit-test string definition and reference handling.
- [x] Unit-test modern uniform signed-int8 arrays.
- [ ] Unit-test uniform arrays for every other supported storage type.
- [ ] Unit-test flattened-array conversion.
- [ ] Unit-test attribute packing and constant pages.
- [ ] Unit-test topology loading.
- [ ] Unit-test polygon and polygon-run loading.
- [ ] Unit-test tiled and constant volume loading.
- [x] Add a compact modern `Polygon_run` semantic round-trip test.
- [x] Add optional Houdini 21/22 Crag integration tests.

### Test framework

- [ ] Select a lightweight maintained C++ test framework or implement a minimal internal assertion harness.
- [ ] Avoid introducing a large dependency solely for simple assertions.
- [ ] Make tests runnable through CTest and directly from the IDE.

## P2 — Expand format compatibility

### Attributes

- [x] Decode modern numeric `values.tuples` arrays.
- [x] Decode modern component-oriented numeric `values.arrays`.
- [x] Promote three-component `P` to the writer's four-component representation.
- [x] Use declared tuple strides when converting `P` and UV into the simplified mesh model.
- [x] Preserve vertex `N` and `uv` exactly in the Crag round-trip.
- [x] Preserve Crag primitive string `name` and integer `piece` exactly.
- [ ] Verify 64-bit integer handling.
- [ ] Verify half-float attribute handling.
- [ ] Add unsigned integer storage where the format uses it.
- [ ] Preserve all supported numeric storage types without forced narrowing.
- [x] Decode indexed string tables, including constant-page-compressed indices.
- [ ] Preserve attribute type information and semantic metadata.
- [x] Support vertex attribute export.
- [ ] Restore and test global attribute export.
- [x] Verify primitive string and integer attribute export for polygon runs.
- [ ] Support attribute packing layouts comprehensively.
- [ ] Support constant-page encodings comprehensively.

### Polygon geometry

- [x] Import Houdini 21/22 `Polygon_run` records with run-length vertex counts.
- [ ] Import all stored polygon primitive groups rather than only the first one.
- [ ] Support mixed triangle and quad meshes without throwing.
- [ ] Support arbitrary n-gons in the simplified conversion or return multiple geometry objects.
- [ ] Preserve open and closed polygon state.
- [ ] Preserve face-varying attributes in a lossless representation.
- [ ] Define a conversion result that can report splits, losses, and unsupported data.

### Modern primitive types

Add each type only with representative Houdini 21 fixtures and tests.

- [ ] Packed geometry
- [ ] Packed fragments
- [ ] Packed disk primitives
- [ ] Curves
- [ ] NURBS and Bezier primitives
- [ ] Spheres and tubes
- [ ] Tetrahedra
- [ ] Height fields
- [ ] Agents and crowds
- [ ] Instancing records
- [ ] OpenVDB primitives

### Compression and wrappers

- [ ] Detect `.bgeo.sc` or other outer compression wrappers.
- [ ] Decide whether decompression belongs in HouIO or a caller-provided stream layer.
- [ ] Add compressed fixture tests if support is implemented.
- [ ] Preserve the parser's ability to operate on arbitrary streams.

### Volumes

- [ ] Verify legacy dense-volume schema behavior in Houdini 21.
- [ ] Add vector field support where represented by multiple volumes or tuple data.
- [ ] Validate all tile metadata before allocation and indexing.
- [ ] Support additional observed compression types.
- [ ] Decide whether modern VDB support should use OpenVDB rather than duplicate it.
- [ ] Avoid automatically converting sparse data into dense memory.

## P2 — Improve the data model

### `Geometry`

- [ ] Mark `Geometry` explicitly as a lossy convenience representation in API docs.
- [ ] Separate point and vertex attribute domains or introduce a second lossless mesh type.
- [ ] Support mixed primitive groups.
- [ ] Remove OpenGL-specific buffer identifiers from the file-format library or isolate them behind an optional module.
- [ ] Add const-correct accessors.
- [ ] Hide mutable implementation members where possible.
- [ ] Validate attribute element counts against geometry counts.

### `Attribute`

- [ ] Replace unsafe reinterpret-cast access with validated typed views or `memcpy`-based access.
- [ ] Resolve alignment and strict-aliasing concerns.
- [ ] Add bounds checks in debug builds.
- [ ] Add immutable views for adapter export.
- [ ] Represent tuple size and storage type with stronger types.
- [ ] Prevent appending values incompatible with the declared component metadata.
- [ ] Add tests for matrices, vectors, scalars, copy, resize, and duplicate-point behavior.

### `Field<T>`

- [ ] Separate file I/O from the field container.
- [ ] Audit coordinate conventions and transform composition.
- [ ] Add overflow-safe allocation checks.
- [ ] Add const sampling APIs.
- [ ] Test boundary behavior and interpolation.
- [ ] Decide whether the custom field format should remain public.

## P2 — Modern C++ and API quality

- [x] Establish C++20 as the experimental language baseline.
- [ ] Replace typedef-style pointer aliases with `using` declarations.
- [ ] Use `nullptr` consistently.
- [ ] Use scoped enums where API compatibility permits.
- [ ] Add `override`, `final`, and `noexcept` where correct.
- [ ] Apply const-correctness consistently.
- [x] Replace the high-level export writer's raw ownership with RAII.
- [ ] Replace remaining raw owning pointers with RAII types.
- [ ] Use `std::span` or an equivalent view abstraction if the language baseline permits it.
- [ ] Remove obsolete commented-out implementation blocks.
- [x] Add a checked-in `.clang-format` configuration.
- [ ] Add static-analysis configuration.
- [ ] Keep formatting-only changes separate from behavior changes.

## P3 — Performance and scalability

- [ ] Measure memory amplification from stream to JSON tree to `HouGeo`.
- [ ] Add benchmarks for large numeric attributes.
- [ ] Add benchmarks for large topology arrays.
- [ ] Add benchmarks for dense volume imports.
- [ ] Avoid per-element `Value` allocations for large arrays.
- [ ] Preserve uniform arrays through semantic loading where possible.
- [ ] Consider direct semantic handlers that avoid constructing a full generic JSON tree.
- [ ] Add streaming or chunked APIs only after compatibility tests are comprehensive.
- [ ] Avoid unnecessary copies between `HouGeo`, adapters, and convenience types.

## P3 — Repository cleanup

### Legacy components

- [ ] Decide whether `scene_exporter/` belongs in this repository.
- [ ] If retained, port the exporter to Python 3 and Houdini 21 APIs.
- [ ] If retained, give it separate documentation, tests, and packaging.
- [ ] Remove or archive the empty `HouScene` stub.
- [ ] Remove or archive the commented-out `ImportHoudini` stub.
- [ ] Remove references to unavailable external frameworks.

### Documentation

- [ ] Add a compatibility matrix by Houdini version and feature.
- [ ] Add generated API documentation if the public surface remains stable.
- [ ] Add contributor guidelines.
- [ ] Add a security policy for malformed file reports.
- [ ] Add release notes once versioned releases begin.
- [ ] Document fixture regeneration steps inside Houdini.

### Releases

- [ ] Define semantic versioning expectations.
- [ ] Create the first fork release only after licensing is resolved.
- [ ] Publish checksums and build provenance for binary artifacts.
- [ ] Do not publish binaries produced from code with unresolved licensing.

## Recommended implementation order

A practical sequence is:

1. Resolve licensing.
2. Repair CMake and establish CI.
3. Convert historical fixtures into real tests.
4. Add minimal Houdini 21 fixtures.
5. Improve diagnostics and parser bounds checking.
6. Remove static export state.
7. Fix confirmed correctness defects.
8. Expand attribute and polygon support.
9. Add modern primitive types one fixture-backed feature at a time.
10. Consider deeper parser and data-model redesign only after behavior is covered.

The main rule is simple: do not rewrite undocumented format knowledge before locking its current behavior down with fixtures and assertions.
