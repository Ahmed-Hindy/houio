# Legacy-Core Modernization Implementation Plan

## Scope

This plan tracks the staged modernization work through branch `refactor/modernization-phases-17-20`, based on the complete Phases 11–16 stack at `e00d4b6`. Integration PR #31 consolidates that stack onto `master`.

The objective is to modernize the retained HouIO core without changing supported file-format behavior, Houdini interoperability, package layout, or documented row-vector matrix semantics. Compatibility aliases are removed only after every in-tree caller has migrated and regression guards are in place.

## Engineering invariants

Every phase must preserve these constraints:

1. Build cleanly with MSVC warnings treated as errors.
2. Keep all CTest tests runnable directly and from IDEs.
3. Preserve binary JSON, geometry, volume, SCF, package-consumer, and Houdini round-trip behavior.
4. Reject overflow, invalid dimensions, invalid indices, null-required dependencies, singular transforms, and malformed input before allocation or mutation where practical.
5. Use value-returning APIs, scoped enums, RAII, `std::span`, fixed-width integers, and standard operator contracts.
6. Remove retired APIs completely after migration; do not accumulate permanent forwarding wrappers.
7. Add source-level regression checks for retired identifiers where accidental reintroduction is plausible.

## Completed work

### Core containers and public APIs

- Modernized `Field<T>` access, transforms, storage handling, and plotting call sites.
- Modernized `Geometry` attribute access and removed `getAttr`, `setAttr`, and `getAttrNames` compatibility methods.
- Added checked grid and sphere generator arithmetic and corrected the 3D grid default primitive type.
- Modernized JSON tree access with value-returning `root`, `value`, `array`, `object`, `keys`, `contains`, and `variant` APIs.
- Removed the complete legacy JSON compatibility surface after migrating `HouGeo` and all other consumers.
- Changed token dispatch from nullable `Parser*` to required `Parser&`.
- Removed adapter compatibility constants, private getter shims, and exporter friendship after migrating every in-tree caller to scoped enums and modern virtual methods.
- Added const-correct adapter accessors and moved geometry conversion and binary export onto immutable adapter views.
- Hardened direct polygon and polygon-run loading against null, empty, and structurally invalid records.
- Added exact scalar/vector field interpolation, clamped-boundary, transform, and non-finite-input coverage.
- Replaced raw tuple-size integers and sentinel storage widths with validated metadata types and optional canonical representations.
- Defined normalized-local, voxel-center, and row-vector field coordinate conventions.
- Made field transform and bound updates strongly exception-safe.
- Corrected voxel spacing to use transformed per-axis basis lengths.
- Added shared dependency-free test assertions and typed exception checks.
- Converted the remaining JSON token and parser-state enums to scoped enums while preserving binary values and compatibility constants.
- Split tiled-volume validation from allocation and decode so malformed metadata is rejected before destination storage is created.
- Completed the ownership and legacy-syntax audit for raw heap operations, `typedef`, `NULL`, and unscoped public enums.

### Math and geometry utilities

- Replaced legacy vector length accessors with `length()` and `squaredLength()`.
- Standardized `Vec2`, `Vec3`, `Vec4`, and `Color` compound-assignment operators to return references.
- Rebuilt `Vec2Algo`, `Vec3Algo`, and `Vec4Algo` around compact value-returning APIs.
- Corrected dormant `Vec4` defects that dropped `w`, omitted `w` from dot products, or reversed scalar division.
- Corrected reflection formulas and added safe orthonormal-basis generation.
- Rebuilt `Matrix22`, `Matrix33`, and `Matrix44` cores and algorithm headers with value-returning inverses/decomposition, scale-aware singularity checks, validated projections, and explicit row-vector rotation semantics.
- Rebuilt bounding-box and ray components with modern names and safer intersection behavior.
- Replaced global math macros with typed C++20 angle conversions using `std::numbers`.
- Reworked fixed-width RGBA packing and non-mutating color conversion.
- Replaced the branch-heavy point-to-triangle routine with a closest-point region algorithm and degenerate fallback.
- Added safe line, plane, triangle, range-remapping, spherical-coordinate, and spherical-interpolation APIs.
- Replaced the pointer-based quadratic solver with `solveQuadratic`, replaced the macro-based RNG with deterministic `RandomGenerator`, and rejected non-finite interpolation inputs.
- Enforced explicit scalar type/component-count layouts for typed `Attribute` access and separated immutable byte views from dirty-marking mutable views.
- Made duplicate-point metadata validation occur before mutation.

### Regression coverage

- Added source guards for retired Field, Geometry, JSON, adapter, attribute, vector, matrix, color, geometry-query, quadratic, and RNG identifiers.
- Expanded math tests for vector contracts, complete 4D arithmetic, reflection, basis construction, matrix accessors, bounding boxes, ray intersections, color packing, spherical conversion, interpolation, range errors, and degenerate geometry.

## Current validation status

- Current exact source: MSVC warnings-as-errors CTest suite passes **19/19**.
- Current exact source: full Release/Houdini matrix passes **47/47**.
- Current exact source: Windows AddressSanitizer matrix passes **19/19**.
- Generated fixtures pass with Houdini **20.0.653** and **22.0.368** as producers and with **20.0.653**, **20.5.410**, **21.0.631**, and **22.0.368** as validators.
- The large Crag round trip and Houdini package matrix pass in all four maintained versions.
- Documentation audit contains no retired API references and now records compatibility, fixture, contribution, field-format, and versioning contracts.
- Phases 11–16 are consolidated for `master` by integration PR #31.
- Phases 17–30 are complete on the active branch; the updated PR #32 head requires CI validation after publication.

## Execution phases

### Phase 1 — Matrix core and algorithms

**Status: complete.**

Target files:

- `include/houio/math/Matrix22.h`
- `include/houio/math/Matrix22Algo.h`
- `include/houio/math/Matrix33.h`
- `include/houio/math/Matrix33Algo.h`
- `include/houio/math/Matrix44.h`
- `include/houio/math/Matrix44Algo.h`
- `tests/test_math.cpp`
- `tests/check_retired_sources.cmake`

Tasks:

- Audit all operators for complete dimensions and conventional operand order.
- Add or standardize `determinant()`, `inverted()`, and singularity behavior.
- Remove redundant free-function aliases and output-parameter helpers.
- Replace `basisFromVector(..., T*, T*)` with a value-returning basis type or reuse the vector basis API.
- Preserve existing row-vector composition semantics explicitly in tests.
- Add tests for multiplication, transpose, determinant, inverse, singular matrices, basis conversion, and transform decomposition helpers.

Exit criteria:

- No public output-pointer matrix helpers remain.
- Strict build and all 19 tests pass.
- Retired matrix identifiers are source-guarded.

### Phase 2 — Adapter interface completion

**Status: complete.**

Target files:

- `include/houio/HouGeoAdapter.h`
- `src/HouGeo.cpp`
- `src/HouGeoIO.cpp`
- adapter tests and inspection tools

Tasks:

- Migrate `HouGeoIO` from private legacy getter shims to modern adapter methods.
- Replace internal `ATTR_TYPE_*`, `ATTR_STORAGE_*`, and primitive aliases with scoped enum values.
- Remove the private/protected compatibility shims after all callers migrate.
- Add compile-time and runtime tests for the final adapter surface.

Exit criteria:

- Scoped enums and modern virtual methods are the only adapter API.
- No compatibility constants or `get*` adapter methods remain.

### Phase 3 — Remaining math utilities

**Status: complete.**

Target files:

- `include/houio/math/Math.h`
- `src/math/Math.cpp`
- `include/houio/math/RNG.h`
- `tests/test_math.cpp`

Tasks:

- Define and test collapsed-edge behavior for `smoothstep`.
- Replace the quadratic output-pointer API with a value-returning result type.
- Audit random-number APIs for deterministic construction, range correctness, and invalid arguments.
- Review interpolation validation, tuple sizing, and boundary behavior.
- Remove obsolete comments and stale copied-algorithm notes.

Exit criteria:

- No remaining public math output-pointer APIs.
- Undefined scalar edge cases are rejected or explicitly documented.
- Strict math tests pass.

### Phase 4 — Container and typed-access audit

**Status: complete.**

Target files:

- `include/houio/Attribute.h`
- `src/Attribute.cpp`
- `include/houio/Field.h`
- `include/houio/Geometry.h`
- corresponding tests

Tasks:

- Audit typed attribute access for alignment and strict-aliasing safety.
- Prefer `std::span`, validated byte views, and `memcpy`-based scalar access.
- Add immutable views for exporter-facing attribute data.
- Verify copy, resize, append, duplicate-point, and metadata mismatch behavior.
- Complete const-correct sampling and access where still missing.

Exit criteria:

- Typed access has explicit layout validation.
- No undefined aliasing assumptions remain in active paths.
- New container tests pass under MSVC and ASan.

### Phase 5 — Full validation and documentation

**Status: complete.**

Tasks:

- [x] Run MSVC warnings-as-errors: **19/19**.
- [x] Run full Release/Houdini matrix: **47/47**.
- [x] Run Windows AddressSanitizer: **19/19**.
- [x] Run CI Linux GCC/UBSan and Clang/fuzzing jobs.
- [x] Review the complete diff for whitespace errors and stale compatibility references.
- [x] Update README examples and API references.
- [x] Update `todo.md` with current status and remaining backlog.
- [x] Create checkpoint `da14f8f`, push the branch, and verify CI run `30082459984`.

Exit criteria:

- All local and CI matrices are green.
- Documentation contains no retired API names.
- Working tree is clean after the checkpoint commit.

### Phase 6 — Const-correct adapter access

**Status: complete.**

Target files:

- `include/houio/HouGeoAdapter.h`
- `include/houio/HouGeo.h`
- `include/houio/HouGeoIO.h`
- `src/HouGeoAdapter.cpp`
- `src/HouGeo.cpp`
- `src/HouGeoIO.cpp`
- adapter and modern-geometry tests

Tasks:

- Add const overloads for point, vertex, primitive, and global attributes.
- Add const overloads for primitive collections and topology.
- Return `shared_ptr<const T>` from const adapter accessors.
- Make geometry conversion accept immutable `HouGeo` and primitive views.
- Make binary export accept an immutable adapter and immutable child views.
- Add compile-time return-type checks and exercise a full round-trip through a const geometry handle.

Exit criteria:

- Export and conversion require no mutable adapter pointers.
- Mutable access remains available only through non-const geometry handles.
- Strict, Release/Houdini, and AddressSanitizer matrices pass.

### Phase 7 — Primitive loading safety

**Status: complete.**

Target files:

- `src/HouGeo.cpp`
- `tests/test_polygon_runs.cpp`

Tasks:

- Validate primitive definition arrays before object conversion and field access.
- Route direct `Poly`, `Polygon_run`, compact run aliases, and legacy `run/Poly` data through schema-path guards.
- Reject missing, non-array, empty, zero-vertex, and zero-primitive records before constructing adapters.
- Validate polygon-run start offsets before topology traversal.
- Add independent semantic round-trip tests for direct `Poly` and legacy `run/Poly` records.
- Add malformed-record tests with precise `primitives[n].definition` and `primitives[n].data` diagnostics.

Exit criteria:

- Malformed primitive records produce diagnostics instead of null dereferences.
- Direct polygon and all supported polygon-run encodings have independent regression coverage.
- Strict, Release/Houdini, and AddressSanitizer matrices pass.

### Phase 8 — Dense-field sampling correctness

**Status: complete and merged.**

Target files:

- `include/houio/Field.h`
- `tests/test_volumes.cpp`

Tasks:

- Verify exact sampling at every voxel center.
- Verify trilinear interpolation at half-cell and arbitrary fractional coordinates.
- Define and test clamped sampling outside each field boundary.
- Cover single-voxel axes and interpolation across 16-voxel tile boundaries.
- Exercise scalar and vector fields through const sampling APIs.
- Verify voxel/world transform round trips at known field bounds.
- Reject NaN and infinite coordinates before floor and integer conversion.

Exit criteria:

- Sampling behavior is explicit for centers, interiors, boundaries, and degenerate axes.
- Non-finite coordinates fail deterministically with `std::invalid_argument`.
- Strict, Release/Houdini, and AddressSanitizer matrices pass.

### Phase 9 — CI and static-analysis maintenance

**Status: complete and merged.**

Target files:

- `.github/workflows/ci.yml`
- `CMakeLists.txt`
- `CMakePresets.json`
- analyzer-reported source locations

Tasks:

- Replace the deprecated Node-based MSVC environment action with direct Visual Studio discovery and initialization.
- Add an independent MSVC `/analyze` configure/build preset and CI matrix entry.
- Treat analyzer diagnostics as errors.
- Resolve analyzer findings without changing supported behavior.

Exit criteria:

- CI no longer depends on `ilammy/msvc-dev-cmd@v1`.
- Native MSVC static analysis is error-clean.
- Existing Linux, Windows, sanitizer, fuzzing, package, and Houdini validation remains green.

### Phase 10 — Strong attribute metadata

**Status: complete and merged.**

Target files:

- `include/houio/HouGeoAdapter.h`
- `include/houio/HouGeo.h`
- `include/houio/Attribute.h`
- `src/HouGeoAdapter.cpp`
- `src/HouGeo.cpp`
- `src/HouGeoIO.cpp`
- adapter, geometry, numeric-storage, and retired-source tests

Tasks:

- Replace adapter tuple-size integers with an explicit positive `TupleSize` value type.
- Keep the value type non-convertible so callers must choose integer or size representations deliberately.
- Replace zero-byte invalid-storage sentinels with `std::optional<std::size_t>`.
- Centralize canonical type and storage names for parsing and serialization.
- Parse storage strings once and convert enums explicitly between lossless and simplified attribute models.
- Remove the duplicate simplified-container string parser.
- Add compile-time contracts, invalid-value tests, canonical-name round trips, and retired-API guards.

Exit criteria:

- Non-positive tuple sizes cannot be represented.
- Invalid storage has no usable byte width or canonical name.
- Export uses the same canonical metadata table as import.
- Strict, Release/Houdini, AddressSanitizer, and static-analysis matrices pass.

### Phase 11 — Lightweight assertion support

**Status: complete; consolidated by integration PR #31.**

Target files:

- `tests/TestSupport.h`
- unit-test translation units
- `tests/check_retired_sources.cmake`

Tasks:

- Add dependency-free shared failure, equality, near-value, and typed exception helpers.
- Remove duplicated local failure functions from unit-test executables.
- Migrate representative exception checks to the typed harness.
- Guard against reintroducing private failure helpers.

Exit criteria:

- Every unit-test executable can use one shared support surface without an external dependency.
- CTest and direct IDE execution remain unchanged.
- Strict, Release/Houdini, AddressSanitizer, and static-analysis matrices pass.

### Phase 12 — Scoped JSON enums

**Status: complete; consolidated by integration PR #31.**

Target files:

- `include/houio/json.h`
- `src/json.cpp`
- `tests/test_binary_json.cpp`
- `tests/check_retired_sources.cmake`

Tasks:

- Convert binary-token identifiers to a byte-backed scoped enum.
- Convert parser states to a scoped enum.
- Preserve established constant names and exact binary values for source compatibility.
- Type raw-byte validation boundaries explicitly.
- Add compile-time non-conversion and binary-value checks.

Exit criteria:

- Token and parser-state values cannot implicitly convert to integers.
- Binary encoding values remain unchanged.
- Public headers contain no unscoped enum declarations.
- All validation matrices pass.

### Phase 13 — Volume metadata before allocation

**Status: complete; consolidated by integration PR #31.**

Target files:

- `include/houio/HouGeo.h`
- `src/HouGeo.cpp`
- `tests/test_volumes.cpp`

Tasks:

- Parse and validate the full tiled-volume representation before allocating a `Field`.
- Validate tile count, object structure, compression metadata, payload kind, and exact payload size in a first pass.
- Decode only validated tile descriptors in a second pass with source and destination bounds checks.
- Reject oversized tile grids before voxel allocation.
- Add adversarial malformed-tile coverage.

Exit criteria:

- Invalid tile metadata cannot cause destination field allocation or indexing.
- Raw and constant tiles preserve current values and ordering.
- Strict, Release/Houdini, AddressSanitizer, and static-analysis matrices pass.

### Phase 14 — Modernization audit closure

**Status: complete; consolidated by integration PR #31.**

Target files:

- `tests/check_retired_sources.cmake`
- `todo.md`
- `implementation-plan.md`

Tasks:

- Verify active code contains no `typedef`, `NULL`, raw heap allocation/deallocation, or unscoped public enums.
- Enforce those findings with source-level regression checks.
- Record that `BloscLibrary` encloses its platform handle with deterministic RAII cleanup.
- Close stale roadmap entries for the test harness, ownership audit, tile validation, aliases, null pointers, and scoped enums.
- Leave broad const-correctness, `noexcept`, span adoption, and comment cleanup open until dedicated audits establish completion.

Exit criteria:

- The roadmap distinguishes completed audits from genuine redesign and compatibility work.
- Source checks fail if the closed legacy patterns return.
- All validation matrices pass.

### Phase 15 — Remove OpenGL-specific core state

**Status: complete; consolidated by integration PR #31.**

Target files:

- `include/houio/Geometry.h`
- active core source audit
- `tests/check_retired_sources.cmake`
- roadmap documents

Tasks:

- Verify the retained geometry container exposes only format-neutral topology and attribute storage.
- Confirm no OpenGL headers, `GLuint` identifiers, buffer targets, or buffer lifecycle calls remain in active core code.
- Keep the existing `indexBuffer()` API because it represents CPU-side mesh topology rather than a graphics-resource identifier.
- Add source-level guards against reintroducing OpenGL-specific buffer ownership into the file-format library.
- Close the stale data-model roadmap entry without adding an optional rendering dependency.

Exit criteria:

- The core library remains rendering-API independent.
- CPU-side topology naming is not incorrectly treated as graphics-resource state.
- Regression guards and all validation matrices pass.

### Phase 16 — Separate field persistence

**Status: complete; consolidated by integration PR #31.**

Target files:

- `include/houio/Field.h`
- `include/houio/FieldIO.h`
- `src/Field.cpp`
- `tests/test_volumes.cpp`
- `tests/check_retired_sources.cmake`

Tasks:

- Remove custom binary load/store members and storage-type state from the `Field<T>` container.
- Introduce nonmember `loadField`, `storeField`, and `storeFieldWithoutBoundingBox` APIs in a dedicated header.
- Preserve the existing on-disk type codes and binary layout.
- Return explicit write success instead of silently ignoring file-open and output failures.
- Migrate in-tree persistence tests and retain malformed/truncated-file rejection.
- Guard against reintroducing persistence members into `Field<T>`.

Exit criteria:

- `Field<T>` owns sampling, transforms, and voxel storage but no custom persistence policy.
- Existing field files remain readable and writable with unchanged type codes and layout.
- The compact no-bounds writer has direct regression coverage.
- Strict, Release/Houdini, AddressSanitizer, and static-analysis matrices pass.

### Phase 17 — Field coordinate and transform contract

**Status: complete locally.**

Target files:

- `include/houio/Field.h`
- `tests/test_volumes.cpp`
- roadmap documents

Tasks:

- Define the field local domain as the normalized cube `[0, 1]^3`.
- Define voxel coordinates as `[0, resolution]` with centers at integer coordinates plus `0.5`.
- Preserve the library's row-vector composition order from voxel to local to world space.
- Correct `voxelSize()` to measure the transformed x, y, and z voxel basis vectors independently.
- Validate finite transforms and finite non-empty bounds.
- Compute inverses and transformed bounds before committing field state.
- Add axis-aligned, rotated, composition, singular-transform, invalid-bound, and non-finite-transform tests.

Exit criteria:

- Per-axis voxel spacing remains positive and rotation-independent.
- Local, voxel, and world transformations compose and invert consistently.
- Failed transform or bound updates leave the previous field state unchanged.
- Strict, Release/Houdini, AddressSanitizer, and static-analysis matrices pass.

### Phase 18 — Explicit hierarchy and exception contracts

**Status: complete locally.**

Target files:

- `include/houio/json.h`
- `tests/test_binary_json.cpp`
- `tests/test_export_safety.cpp`
- roadmap documents

Tasks:

- Remove redundant `virtual` specifiers from concrete JSON writer overrides.
- Add missing `override` declarations to every `JSONLogger` handler implementation.
- Keep concrete JSON handlers `final` and enforce that contract at compile time.
- Mark local adapter and stream-buffer test doubles `final`.
- Audit public non-throwing accessors and retain `noexcept` only where allocation, checked access, user-defined operations, or stream work cannot invalidate the guarantee.

Exit criteria:

- Every concrete JSON handler implementation is compiler-checked as an override.
- Concrete JSON handlers and test doubles cannot be accidentally subclassed.
- No broad or mechanically unsafe `noexcept` annotations are introduced.
- Strict, Release/Houdini, AddressSanitizer, and static-analysis matrices pass.

### Phase 19 — Retired implementation-comment cleanup

**Status: complete locally.**

Target files:

- `src/json.cpp`
- `tests/check_retired_sources.cmake`
- roadmap documents

Tasks:

- Remove the remaining commented-out parser return statement.
- Replace stale implementation comments that referenced removed internal types.
- Correct and compact the binary-length encoding documentation.
- Verify active source contains no `#if 0` implementation blocks.
- Add regression checks for preprocessor-disabled and clearly commented-out control-flow or ownership statements.

Exit criteria:

- Active source contains no obsolete commented-out implementation blocks.
- Maintained explanatory comments describe the current C++ implementation and format behavior.
- The retired-source test rejects disabled code and clear commented-code patterns.
- Strict, Release/Houdini, AddressSanitizer, and static-analysis matrices pass.

### Phase 20 — Houdini compatibility and maintained project contracts

**Status: complete locally.**

Target files:

- `include/houio/HouGeoAdapter.h`
- `include/houio/HouGeo.h`
- `include/houio/FieldIO.h`
- `src/HouGeoAdapter.cpp`
- `src/HouGeo.cpp`
- `src/HouGeoIO.cpp`
- `tests/test_polygon_runs.cpp`
- `tests/test_conversion_safety.cpp`
- `tools/houdini/generate_fixture_suite.py`
- `tools/houdini/validate_fixture_suite.py`
- `tools/houdini/run_fixture_roundtrips.ps1`
- `tools/houdini/run_crag_roundtrip.ps1`
- `tools/houdini/run_package_matrix.ps1`
- README, contributor, compatibility, fixture, field-format, and versioning documents

Tasks:

- Make Houdini 20.0.653 a supported fixture producer, not only a candidate validator.
- Preserve indexed string attributes with tuple sizes greater than one.
- Replace the scalar-only paged integer-index decoder with a tuple-, packing-, and constant-page-aware decoder.
- Extend the adapter string API with element/component addressing while preserving scalar callers.
- Export string tuples using the declared tuple size and flattened string-index count.
- Add global string-tuple regression coverage for the Houdini 20.0 `varmap` schema.
- Add string tuple bounds, incomplete-tuple, and strong mutation-safety tests.
- Extend generated fixtures with multi-page point strings and vertex-domain strings.
- Make the Houdini validator compatible with the 20.0 HOM surface without weakening comparisons available in later versions.
- Reject unknown `known_losses` manifest keys.
- Maintain executable four-version fixture, Crag, and package matrix commands.
- Document exact supported builds, recognized unsupported records, intentional-loss policy, fixture regeneration, contribution requirements, semantic versioning, and release blockers.
- Keep `FieldIO` installed and opt-in while marking its current native binary layout experimental and non-portable.

Exit criteria:

- The 14-fixture suite passes when generated by Houdini 20.0.653 and 22.0.368, then validated in Houdini 20.0.653, 20.5.410, 21.0.631, and 22.0.368.
- Point, vertex, primitive, and global string attributes round-trip, including multi-page point indices and multi-component global tuples.
- The Crag asset preserves exact topology, exact tested integer/string attributes, and zero tested numeric drift in all four versions.
- The generated Houdini package passes headlessly in all four versions.
- Compatibility claims and experimental-format status are documented and executable.
- Strict, Release/Houdini, AddressSanitizer, and static-analysis matrices pass.

### Phase 21 — Const source and non-owning range completion

**Status: complete.**

Target files:

- `include/houio/Field.h`
- `include/houio/Geometry.h`
- `src/Geometry.cpp`
- `tests/test_geometry_container.cpp`
- `tests/test_volumes.cpp`
- `tests/check_retired_sources.cmake`
- roadmap documents

Tasks:

- Replace field conversion from a mutable shared pointer with a const source reference.
- Preserve field resolution, bound, and values when converting from an immutable source.
- Replace `Geometry::merge(const std::vector<Ptr>&)` with non-owning span overloads for mutable and immutable shared pointers.
- Internally convert every merge source to `Geometry::ConstPtr` before reading attributes or topology.
- Exercise both span overloads with fixed-size arrays.
- Guard against the retired mutable-pointer field conversion and vector-coupled merge signatures.
- Retain `vector<bool>` group membership APIs because its proxy representation cannot form a safe contiguous `std::span<bool>`.
- Retain output vectors where the callee owns resizing or produces storage for the caller.

Exit criteria:

- Field conversion requires no mutable source handle and has no nullable input state.
- Geometry merging accepts caller-owned contiguous ranges without requiring a `std::vector`.
- Both mutable and immutable geometry pointer ranges merge identically.
- Remaining vector references have an explicit ownership or proxy-storage reason.
- Strict, Release/Houdini, AddressSanitizer, and static-analysis matrices pass.

### Phase 22 — Opt-in performance baselines

**Status: complete.**

Target files:

- `CMakeLists.txt`
- `CMakePresets.json`
- `benchmarks/houio_benchmarks.cpp`
- `docs/benchmarks.md`
- README and roadmap documents

Tasks:

- Add `HOUIO_BUILD_BENCHMARKS` as an opt-in build feature with no external dependency.
- Add a warnings-as-errors MSVC benchmark preset.
- Measure typed three-component numeric attribute writes and reads.
- Measure checked triangle-grid generation and complete index traversal.
- Measure in-memory constant dense-volume import through `HouGeoIO`.
- Run every workload repeatedly and report median time, throughput, and an observable checksum.
- Add configurable workload sizes and CSV output.
- Keep timing thresholds out of CTest and CI to avoid hardware-dependent failures.
- Document repeatable comparison methodology and explicitly retain peak-memory amplification as open work.

Exit criteria:

- The benchmark target builds cleanly under MSVC `/W4 /WX`.
- Reduced smoke workloads execute all three baselines successfully.
- Default workloads are configurable without source changes.
- Benchmark results are clearly separated from correctness tests and compatibility claims.
- Existing strict, Release/Houdini, AddressSanitizer, and static-analysis matrices remain green.

### Phase 23 — Structured simplified-conversion reporting

**Status: complete.**

Target files:

- `include/houio/HouGeoIO.h`
- `src/HouGeoIO.cpp`
- `tests/test_conversion_safety.cpp`
- README, compatibility, onboarding, and roadmap documents

Tasks:

- Add `GeometryConversionReport` and `GeometryConversionResult` without removing existing convenience conversions.
- Preserve structured diagnostics in the result for conversion failures.
- Record source and output point counts.
- Record distinct source points requiring face-varying splits and the actual number of duplicated output points.
- Record skipped point, vertex, primitive, and global attributes.
- Record dropped point, vertex, and primitive groups.
- Record Houdini-to-simplified winding reversal.
- Exercise a successful conversion with non-numeric attributes, unsupported simplified domains, and groups.
- Exercise a real two-triangle UV seam that duplicates one shared point.
- Exercise a schema failure that returns diagnostics and partial report metadata.

Exit criteria:

- Successful lossy conversions expose every currently known simplified-model loss category.
- Face-varying point splitting is quantified deterministically.
- Failed conversions return no geometry but preserve diagnostics and available source metadata.
- Existing `convertToGeometry` overloads retain their behavior.
- Strict, Release/Houdini, AddressSanitizer, and static-analysis matrices pass.

### Phase 24 — Attribute definition metadata preservation

**Status: complete.**

Target files:

- `include/houio/HouGeoAdapter.h`
- `include/houio/HouGeo.h`
- `src/HouGeoAdapter.cpp`
- `src/HouGeo.cpp`
- `src/HouGeoIO.cpp`
- `tests/test_modern_geometry.cpp`
- README, compatibility, and roadmap documents

Tasks:

- Add adapter-level accessors for attribute definition scope and semantic options.
- Keep custom adapters source-compatible through public-scope and empty-options defaults.
- Preserve arbitrary non-empty scope strings from parsed attribute definitions.
- Preserve the complete nested JSON `options` object rather than selecting known keys.
- Export retained metadata instead of hardcoding `public` and an empty map.
- Normalize programmatic null options to an owned empty object.
- Reject empty programmatic scopes.
- Exercise non-empty nested semantic options and a non-public scope through import, binary export, and re-import.

Exit criteria:

- Attribute scope survives faithful `HouGeo` round trips.
- Nested option values and unknown option keys survive without interpretation or loss.
- Existing adapters that do not provide metadata continue to export public attributes with empty options.
- Strict, Release/Houdini, AddressSanitizer, and static-analysis matrices pass.

### Phase 25 — Mixed primitive-group and storage encapsulation audit

**Status: complete.**

Target files:

- `src/HouGeo.cpp`
- `tests/test_groups.cpp`
- README, compatibility, and roadmap documents

Tasks:

- Reject empty programmatic group names.
- Reject point, vertex, and primitive memberships that do not match their active domains.
- Construct a faithful mixed-record geometry containing one polygon and one dense volume.
- Round-trip independent primitive groups selecting each record type.
- Confirm primitive order, counts, topology, and membership remain stable.
- Audit `HouAttribute`, `HouTopology`, `HouVolume`, `HouPoly`, `HouGeo`, `Geometry`, and `Attribute` storage visibility.
- Confirm mutable storage remains private and external access uses validated mutation or immutable views.

Exit criteria:

- Invalid programmatic group state is rejected before export.
- Mixed polygon/volume primitive groups survive binary export and import.
- Maintained implementation containers expose no public mutable storage.
- Strict, Release/Houdini, AddressSanitizer, and static-analysis matrices pass.

### Phase 26 — Explicit geometry-model names

**Status: complete.**

Target files:

- `include/houio/GeometryModels.h`
- `tests/package_consumer/main.cpp`
- README, compatibility, onboarding, and roadmap documents

Tasks:

- Add `HoudiniGeometry` as a non-breaking alias for the supported Houdini-oriented `HouGeo` model.
- Add `SimplifiedMesh` as a non-breaking alias for the render-oriented `Geometry` model.
- Keep established class names and APIs intact.
- Document the distinct domain and conversion guarantees next to the aliases.
- Compile and execute an installed-package consumer using the new names.
- Add compile-time alias identity checks.

Exit criteria:

- Public type names communicate which model is Houdini-oriented and which is simplified.
- Existing source remains compatible.
- The new header is installed and usable through `find_package(houio)`.
- Strict, Release/Houdini, AddressSanitizer, and static-analysis matrices pass.

### Phase 27 — Zero-copy topology export view

**Status: complete.**

Target files:

- `include/houio/HouGeoAdapter.h`
- `include/houio/HouGeo.h`
- `src/HouGeoAdapter.cpp`
- `src/HouGeo.cpp`
- `src/HouGeoIO.cpp`
- `tests/test_export_safety.cpp`
- roadmap documents

Tasks:

- Add an optional immutable topology index view to the adapter contract.
- Keep `indexValues()` as the required compatibility API for existing custom adapters.
- Expose `HouTopology` owned indices directly through the immutable view.
- Validate that the selected view or fallback copy matches `indexCount()`.
- Write 32-bit topology arrays directly from the immutable view.
- Retain 16-bit compaction where every index fits.
- Test that view-enabled adapters perform no `indexValues()` copy.
- Test that legacy adapters without a view perform one fallback copy and preserve values.

Exit criteria:

- Faithful `HouTopology` export no longer allocates a full duplicate 32-bit index vector.
- Existing custom topology adapters remain source-compatible.
- Index count mismatches and negative indices remain rejected.
- Strict, Release/Houdini, AddressSanitizer, and static-analysis matrices pass.

### Phase 28 — Zero-copy primitive-container export view

**Status: complete.**

Target files:

- `include/houio/HouGeoAdapter.h`
- `include/houio/HouGeo.h`
- `src/HouGeoAdapter.cpp`
- `src/HouGeo.cpp`
- `src/HouGeoIO.cpp`
- `tests/test_export_safety.cpp`
- roadmap documents

Tasks:

- Add an optional immutable primitive pointer-list view to the adapter contract.
- Keep both established `primitives()` vector APIs for source compatibility.
- Expose `HouGeo` owned primitive pointers directly through the immutable view.
- Convert mutable shared pointers to const primitive views before serialization.
- Validate every primitive pointer and supported record type.
- Validate the summed record primitive count against declared `primitiveCount()`.
- Validate the summed topology vertices against declared `vertexCount()`.
- Check topology-offset and primitive-count arithmetic before narrowing or addition.
- Test that view-enabled adapters perform no `primitives()` copy.
- Test that legacy adapters without a view perform one fallback copy and preserve polygon records.

Exit criteria:

- Faithful `HouGeo` export no longer allocates a duplicate primitive shared-pointer vector.
- Existing custom geometry adapters remain source-compatible.
- Null, unsupported, count-mismatched, and topology-mismatched primitive adapters are rejected.
- Strict, Release/Houdini, AddressSanitizer, and static-analysis matrices pass.

### Phase 29 — Single n-gon simplified conversion

**Status: complete.**

Target files:

- `src/HouGeoIO.cpp`
- `tests/test_conversion_safety.cpp`
- README, compatibility, and roadmap documents

Tasks:

- Reuse the simplified model's existing single-polygon storage for one arbitrary n-gon.
- Preserve point attributes, vertex-to-point splitting, winding conversion, and conversion reporting.
- Keep fixed-size line, triangle, and quad conversion unchanged.
- Reject multiple n-gons because `SimplifiedMesh` cannot encode multiple variable-size polygon boundaries.
- Return a structured `unsupported_input` diagnostic for the multi-n-gon case.
- Verify a five-vertex polygon's type, count, topology, point data, and winding reversal.

Exit criteria:

- A single polygon with more than four vertices converts successfully.
- The simplified result reports one polygon and the exact vertex count.
- Multiple n-gons remain explicitly rejected rather than flattened ambiguously.
- Strict, Release/Houdini, AddressSanitizer, and static-analysis matrices pass.

### Phase 30 — Open-polygon closure-loss reporting

**Status: complete.**

Target files:

- `include/houio/HouGeoIO.h`
- `src/HouGeoIO.cpp`
- `tests/test_conversion_safety.cpp`
- README, compatibility, onboarding, and roadmap documents

Tasks:

- Detect open polygon runs with three or more vertices during simplified conversion.
- Leave two-vertex line conversion unaffected because line closure is not represented as a face boundary.
- Add `polygonClosureLost` to the structured conversion report.
- Emit a warning diagnostic at `conversion.primitive.closed`.
- Continue conversion because the simplified result remains useful when the loss is accepted explicitly.
- Exercise the flag and warning through an open five-vertex polygon conversion.

Exit criteria:

- Open-face closure loss is never silent when using `convertToGeometryResult`.
- Successful conversion retains diagnostics and report metadata.
- Closed polygon conversion behavior remains unchanged.
- Strict, Release/Houdini, AddressSanitizer, and static-analysis matrices pass.

## Deferred work

The following remain separate from this modernization branch unless required by a discovered defect:

- New primitive-record support.
- Lossless point/vertex-domain redesign.
- Native sparse OpenVDB public adapters.
- Performance architecture changes that bypass the JSON tree.
- Project-wide licensing and third-party provenance resolution.

Licensing and provenance remain release blockers even after technical validation succeeds.
