# Legacy-Core Modernization Implementation Plan

## Scope

This plan tracks the active modernization work on branch `refactor/modernize-legacy-core`, based on commit `16c485a`.

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
- Documentation audit contains no retired API references.
- CI run `30082459984` passes Linux GCC Release, GCC/UBSan, Clang parser fuzzing, MSVC warnings-as-errors, MSVC Release, and MSVC AddressSanitizer.
- CI reports non-failing Node.js deprecation annotations for `ilammy/msvc-dev-cmd@v1`; action maintenance is tracked separately.

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

## Deferred work

The following remain separate from this modernization branch unless required by a discovered defect:

- New primitive-record support.
- Lossless point/vertex-domain redesign.
- Native sparse OpenVDB public adapters.
- Performance architecture changes that bypass the JSON tree.
- Project-wide licensing and third-party provenance resolution.

Licensing and provenance remain release blockers even after technical validation succeeds.
