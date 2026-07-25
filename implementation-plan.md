# Legacy-Core Modernization Implementation Plan

## Scope

This plan tracks the staged modernization work through branch `test/dense-field-sampling`, based on `origin/master` commit `f501032`.

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
- CI validation for the active branch is pending until it is pushed.
- Replacement of the deprecated MSVC environment action remains the next independent phase.

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

**Status: complete locally.**

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

## Deferred work

The following remain separate from this modernization branch unless required by a discovered defect:

- New primitive-record support.
- Lossless point/vertex-domain redesign.
- Native sparse OpenVDB public adapters.
- Performance architecture changes that bypass the JSON tree.
- Project-wide licensing and third-party provenance resolution.

Licensing and provenance remain release blockers even after technical validation succeeds.
