# Roadmap

This roadmap contains current work only. Completed migration history is intentionally omitted.

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
- [ ] Add matrix, scalar, copy, resize, and duplicate-point tests for `Attribute` and `Geometry`.
- [ ] Unit-test every supported binary token type.
- [ ] Unit-test binary integer length encodings.
- [ ] Unit-test string definition and reference handling.
- [ ] Unit-test every supported uniform numeric array type.
- [ ] Unit-test direct polygon and polygon-run loading independently.
- [ ] Introduce a small assertion harness or lightweight maintained C++ test framework.
- [ ] Keep all tests runnable through CTest and directly from IDEs.

### Compiler quality

- [ ] Resolve remaining anonymous-union, parser, and dense-field warnings where API compatibility permits.
- [ ] Remove unreachable code warnings in the JSON implementation.
- [ ] Add a strict warnings-as-errors CI job after the warning baseline is clean.
- [ ] Add static-analysis configuration.

### Public API

- [ ] Add const-correct accessors across geometry and field types.
- [ ] Replace remaining raw owning pointers with RAII types.
- [ ] Replace unsafe typed attribute access with validated views or `memcpy`-based operations.
- [ ] Resolve alignment and strict-aliasing risks in typed `get()` and `set()` methods.
- [ ] Add immutable attribute views for adapter export.
- [ ] Represent tuple size and storage type with stronger types.
- [ ] Reject appended values that do not match declared component metadata.

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
