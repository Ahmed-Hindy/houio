# Roadmap and Current Work

The long-term roadmap remains below. The active modernization program is summarized first so completed work, validation state, and the immediate execution order are visible in one place. Detailed implementation steps are maintained in `implementation-plan.md`.

## Active branch

- Branch: `refactor/modernization-phases-17-20`
- Baseline commit: `e00d4b6` (complete Phases 11–16 stack; integration PR #31 targets `master`).
- Current exact source: MSVC warnings-as-errors suite passes **19/19**.
- Current exact source: full Release/Houdini matrix passes **47/47**.
- Current exact source: Windows AddressSanitizer matrix passes **19/19**.
- Generated fixtures, the large Crag asset, and the Houdini package pass with **20.0.653**, **20.5.410**, **21.0.631**, and **22.0.368**.
- Phases 11–16 are consolidated for `master` by integration PR #31.
- Current branch completes field/API cleanup, Houdini 20.0 producer compatibility, indexed string tuples, and maintained project contracts.

## Modernization completed

- [x] Modernize `Field<T>` access, storage, transforms, and call sites.
- [x] Modernize `Geometry` attribute access and remove legacy forwarding methods.
- [x] Add checked generator arithmetic and overflow tests.
- [x] Modernize the JSON tree API and remove all legacy JSON compatibility methods.
- [x] Make parser token dispatch non-null by construction.
- [x] Restrict adapter compatibility aliases from public consumers.
- [x] Modernize vector lengths, compound operators, and complete `Vec4` arithmetic.
- [x] Replace legacy vector algorithm headers and correct reflection/basis behavior.
- [x] Modernize bounding boxes, rays, color packing, angle conversion, and spherical conversion.
- [x] Replace the legacy point-to-triangle algorithm and harden line/plane/range interpolation utilities.
- [x] Add source-level guards for retired APIs and broaden math regression coverage.
- [x] Complete binary JSON scalar-token, length-width, string-token, uniform-array, and null round-trip coverage.
- [x] Add packed-bool and UInt16 uniform-array writer/reader support.
- [x] Preserve parsed uniform arrays when serializing JSON trees.
- [x] Serialize signed and unsigned 8-bit ASCII values numerically.
- [x] Preserve null values in exported adapter dictionaries.
- [x] Add const-correct adapter accessors for attributes, primitives, and topology.
- [x] Move geometry conversion and binary export onto immutable adapter views.
- [x] Harden direct polygon and polygon-run loading against malformed JSON records.
- [x] Add independent direct `Poly`, legacy `run/Poly`, and polygon-run regression coverage.
- [x] Add exact scalar/vector field sampling, boundary, interpolation, and transform coverage.
- [x] Reject non-finite field sampling coordinates before floor/integer conversion.
- [x] Introduce validated tuple-size metadata and canonical attribute type/storage metadata helpers.
- [x] Add a dependency-free shared test assertion harness and retire duplicated failure helpers.
- [x] Convert the remaining public JSON token and parser-state enums to scoped enums.
- [x] Validate complete tiled-volume metadata before allocating destination field storage.
- [x] Audit active ownership: no raw heap allocation/deallocation remains; the platform library handle is enclosed by `BloscLibrary` RAII.
- [x] Move custom field persistence out of the `Field<T>` container into a dedicated nonmember I/O API.
- [x] Define and test normalized-local, voxel-center, and row-vector field transform conventions.
- [x] Make field transform and bound updates strongly exception-safe.
- [x] Report voxel spacing from transformed basis-vector lengths.
- [x] Make concrete JSON handlers and test adapters explicit `final` classes with complete `override` declarations.
- [x] Complete the bounded `noexcept` audit without annotating allocating or checked-access APIs incorrectly.
- [x] Remove obsolete commented-out parser implementation and guard against disabled code blocks returning.
- [x] Preserve indexed string tuples across point, vertex, primitive, and global domains.
- [x] Validate multi-page string indices and Houdini 20.0 `varmap` tuple encoding.
- [x] Run fixture, large-asset, and package matrices across Houdini 20.0, 20.5, 21.0, and 22.0.
- [x] Define maintained compatibility, fixture, contribution, versioning, and experimental field-format contracts.

## Modernization next

- [x] Complete `Matrix22`, `Matrix33`, and `Matrix44` core and algorithm modernization.
- [x] Replace output-pointer matrix basis helpers with value-returning APIs.
- [x] Migrate `HouGeoIO` off private adapter getter shims and remove adapter compatibility aliases.
- [x] Replace the quadratic output-pointer API and audit `smoothstep`, RNG, and interpolation edge cases.
- [x] Audit typed `Attribute` access for alignment and strict-aliasing safety.
- [x] Add remaining container copy, resize, duplicate-point, and immutable-view tests.
- [x] Rerun strict, Release/Houdini, and AddressSanitizer matrices locally.
- [x] Update README examples and audit documentation for retired API names.
- [x] Create and push checkpoint commit `da14f8f`.
- [x] Verify Linux GCC/UBSan and Clang/fuzzing CI matrices.

Priority levels:

- **P0** — blocks distribution or responsible release
- **P1** — required for a dependable maintained library
- **P2** — compatibility and data-model expansion
- **P3** — optimization and optional redesign

## P0 — Distribution readiness

### Licensing and provenance

- [ ] Establish a project-wide license before distributing source or binaries.
- [ ] Document the license and provenance of code under `include/ttl/`.
- [ ] Document the license and provenance of the half-float implementation.
- [ ] Add the required notices to source and binary distributions.
- [ ] Do not publish releases until these items are complete.

## P1 — Maintained baseline

### Houdini 20.0+ compatibility

- [x] Add generated compatibility coverage using Houdini 20.0 as a fixture producer.
- [x] Keep package validation active for 20.0, 20.5, 21.0, and 22.0.
- [x] Document recognized records that remain unsupported.
- [x] Record intentional round-trip losses in fixture manifests and reject unknown loss keys.

### Tests

- [x] Add exact dense-volume boundary and interpolation tests.
- [x] Add matrix, scalar, copy, resize, and duplicate-point tests for `Attribute` and `Geometry`.
- [x] Unit-test every supported binary token type.
- [x] Unit-test binary integer length encodings.
- [x] Unit-test string definition and reference handling.
- [x] Unit-test every supported uniform numeric array type.
- [x] Unit-test direct polygon and polygon-run loading independently.
- [x] Introduce a small dependency-free assertion harness shared by the unit tests.
- [x] Keep all tests runnable through CTest and directly from IDEs.

### Compiler quality

- [x] Resolve remaining anonymous-union, parser, and dense-field warnings where API compatibility permits.
- [x] Remove unreachable code warnings in the JSON implementation.
- [x] Add a strict warnings-as-errors CI job after the warning baseline is clean.
- [x] Replace the deprecated Node-based MSVC environment action with direct `vswhere`/`VsDevCmd.bat` initialization.
- [x] Add an error-clean native MSVC `/analyze` configuration and CI preset.

### Public API

- [x] Add const-correct accessors across geometry, field, and adapter types.
- [x] Complete the ownership audit: active heap ownership uses standard RAII, and the dynamic-library handle is enclosed by `BloscLibrary`.
- [x] Replace unsafe typed attribute access with validated views or `memcpy`-based operations.
- [x] Resolve alignment and strict-aliasing risks in typed `get()` and `set()` methods.
- [x] Add immutable attribute views for adapter export.
- [x] Represent tuple size and storage type with stronger types.
- [x] Reject appended values that do not match declared component metadata.

## P2 — Format compatibility

### Attributes

- [ ] Add unsigned integer storage where the file format uses it.
- [ ] Preserve complete attribute type and semantic metadata.
- [x] Extend string-table coverage to every supported domain and maintained page layout.

### Geometry

- [ ] Preserve point and vertex domains in a lossless mesh representation.
- [ ] Support mixed primitive groups.
- [ ] Support arbitrary n-gons in simplified conversion or return multiple geometry objects.
- [ ] Preserve face-varying attributes without forced point duplication.
- [ ] Add a conversion result that reports splits, losses, and unsupported data.

### Primitive records

Add fixture-backed support one record type at a time:

- [ ] Packed geometry
- [ ] Packed fragments
- [ ] Packed disk primitives
- [ ] NURBS and Bezier curves
- [ ] Spheres and tubes
- [ ] Tetrahedra
- [ ] Height fields
- [ ] Agents and crowds
- [ ] Instancing records
- [ ] Native sparse OpenVDB primitives

### Volumes and compression

- [x] Validate all tile metadata before allocation and indexing.
- [ ] Add vector-field support where represented by multiple volumes or tuple data.
- [ ] Support additional observed compression types.
- [ ] Investigate additional outer wrappers only when representative fixtures are available.
- [ ] Define an optional public OpenVDB adapter with sparse-grid preservation.

## P2 — Data model

### Geometry

- [x] Verify the file-format library contains no OpenGL-specific buffer identifiers and guard against their reintroduction.
- [ ] Hide mutable implementation details where practical.
- [ ] Make lossless and simplified geometry responsibilities explicit in type names and APIs.

### Field

- [x] Separate custom binary persistence from the field container through `FieldIO.h`.
- [x] Audit coordinate conventions and transform composition.
- [x] Add const sampling APIs.
- [x] Keep the custom field I/O API public and opt-in while explicitly classifying its native on-disk layout as experimental.

### Modern C++

- [x] Replace typedef-style aliases with `using` declarations.
- [x] Use `nullptr` consistently.
- [x] Use scoped enums in public headers where source compatibility permits.
- [x] Add `override`, `final`, and `noexcept` where correct.
- [ ] Apply const-correctness consistently.
- [ ] Use `std::span` or equivalent views for non-owning ranges.
- [x] Remove obsolete commented-out implementation blocks.

## P3 — Performance

- [ ] Measure memory amplification from input stream to JSON tree to `HouGeo`.
- [ ] Benchmark large numeric attributes.
- [ ] Benchmark large topology arrays.
- [ ] Benchmark dense-volume imports.
- [ ] Avoid per-element `Value` allocations for large arrays.
- [ ] Preserve uniform arrays through semantic loading where possible.
- [ ] Evaluate direct semantic handlers that bypass the generic JSON tree.
- [ ] Add streaming or chunked APIs only after compatibility tests are comprehensive.
- [ ] Reduce copies between `HouGeo`, adapters, and simplified representations.

## Documentation and releases

- [x] Add a maintained Houdini-version and feature compatibility matrix.
- [x] Add contributor guidelines.
- [x] Document fixture regeneration steps.
- [x] Define semantic-versioning expectations.
- [ ] Add release notes when versioned releases begin.
- [ ] Publish checksums and build provenance with every binary artifact.
