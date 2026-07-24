# Roadmap and Current Work

The long-term roadmap remains below. The active modernization program is summarized first so completed work, validation state, and the immediate execution order are visible in one place. Detailed implementation steps are maintained in `implementation-plan.md`.

## Active branch

- Branch: `test/complete-binary-json-coverage`
- Baseline commit: `7d09bdb`
- Current exact source: MSVC warnings-as-errors suite passes **19/19**.
- Current exact source: full Release/Houdini matrix passes **47/47**.
- Current exact source: Windows AddressSanitizer matrix passes **19/19**.
- Branch CI run `30087208906` passes Linux GCC Release, GCC/UBSan, Clang parser fuzzing, MSVC warnings-as-errors, MSVC Release, and MSVC AddressSanitizer.
- CI reports non-failing Node.js deprecation annotations for `ilammy/msvc-dev-cmd@v1`.

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

- [ ] Add generated compatibility coverage using Houdini 20.0 as a fixture producer.
- [ ] Keep package validation active for 20.0, 20.5, 21.0, and 22.0.
- [ ] Document recognized records that remain unsupported.
- [ ] Record intentional round-trip losses in fixture manifests.

### Tests

- [ ] Add exact dense-volume boundary and interpolation tests.
- [x] Add matrix, scalar, copy, resize, and duplicate-point tests for `Attribute` and `Geometry`.
- [x] Unit-test every supported binary token type.
- [x] Unit-test binary integer length encodings.
- [x] Unit-test string definition and reference handling.
- [x] Unit-test every supported uniform numeric array type.
- [ ] Unit-test direct polygon and polygon-run loading independently.
- [ ] Introduce a small assertion harness or lightweight maintained C++ test framework.
- [x] Keep all tests runnable through CTest and directly from IDEs.

### Compiler quality

- [x] Resolve remaining anonymous-union, parser, and dense-field warnings where API compatibility permits.
- [x] Remove unreachable code warnings in the JSON implementation.
- [x] Add a strict warnings-as-errors CI job after the warning baseline is clean.
- [ ] Replace or update `ilammy/msvc-dev-cmd@v1` before GitHub removes forced Node.js 24 compatibility.
- [ ] Add static-analysis configuration.

### Public API

- [ ] Add const-correct accessors across geometry and field types.
- [ ] Replace remaining raw owning pointers with RAII types.
- [x] Replace unsafe typed attribute access with validated views or `memcpy`-based operations.
- [x] Resolve alignment and strict-aliasing risks in typed `get()` and `set()` methods.
- [x] Add immutable attribute views for adapter export.
- [ ] Represent tuple size and storage type with stronger types.
- [x] Reject appended values that do not match declared component metadata.

## P2 — Format compatibility

### Attributes

- [ ] Add unsigned integer storage where the file format uses it.
- [ ] Preserve complete attribute type and semantic metadata.
- [ ] Extend string-table coverage to every supported domain and page layout.

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

- [ ] Validate all tile metadata before allocation and indexing.
- [ ] Add vector-field support where represented by multiple volumes or tuple data.
- [ ] Support additional observed compression types.
- [ ] Investigate additional outer wrappers only when representative fixtures are available.
- [ ] Define an optional public OpenVDB adapter with sparse-grid preservation.

## P2 — Data model

### Geometry

- [ ] Remove OpenGL-specific buffer identifiers from the file-format library or isolate them in an optional module.
- [ ] Hide mutable implementation details where practical.
- [ ] Make lossless and simplified geometry responsibilities explicit in type names and APIs.

### Field

- [ ] Separate file I/O concerns from the field container.
- [ ] Audit coordinate conventions and transform composition.
- [ ] Add const sampling APIs.
- [ ] Define whether the custom field format remains public.

### Modern C++

- [ ] Replace typedef-style aliases with `using` declarations.
- [ ] Use `nullptr` consistently.
- [ ] Use scoped enums where source compatibility permits.
- [ ] Add `override`, `final`, and `noexcept` where correct.
- [ ] Apply const-correctness consistently.
- [ ] Use `std::span` or equivalent views for non-owning ranges.
- [ ] Remove obsolete commented-out implementation blocks.

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

- [ ] Add a maintained Houdini-version and feature compatibility matrix.
- [ ] Add contributor guidelines.
- [ ] Document fixture regeneration steps.
- [ ] Define semantic-versioning expectations.
- [ ] Add release notes when versioned releases begin.
- [ ] Publish checksums and build provenance with every binary artifact.
