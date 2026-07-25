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
- Houdini fixture validation passes with Houdini **21.0.631** and **22.0.368**.
- Documentation audit contains no retired API references.
- Phases 8–10 are merged into `master`.
- Stacked PRs #25–#27 contain Phases 11–13; the active audit phase will be published above them.
- CI validation for the active stacked branches is tracked separately from the completed local matrices.

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

## Deferred work

The following remain separate from this modernization branch unless required by a discovered defect:

- New primitive-record support.
- Lossless point/vertex-domain redesign.
- Native sparse OpenVDB public adapters.
- Performance architecture changes that bypass the JSON tree.
- Project-wide licensing and third-party provenance resolution.

Licensing and provenance remain release blockers even after technical validation succeeds.
