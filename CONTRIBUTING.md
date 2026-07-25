# Contributing to HouIO

HouIO accepts focused changes that preserve explicit format behavior, deterministic diagnostics, and the maintained compatibility matrix.

> [!IMPORTANT]
> The repository does not yet have a project-wide license. Do not redistribute the source, generated packages, or binaries. Contributions must have clear authorship and must not copy code whose license or provenance cannot be documented.

Start with [Developer onboarding](onboard.md) for the architecture, build presets, debugging workflow, and core invariants.

## Choose a narrow change

Keep parser, schema, data-model, package, documentation, and performance work separate when practical. A pull request should have one primary purpose and enough tests to demonstrate that purpose independently.

For new format support, provide a representative Houdini-generated fixture before expanding the parser or semantic model. Do not infer undocumented record behavior from names alone.

## Branch and commit expectations

- Branch from the current complete `master` state.
- Use descriptive branch names such as `fix/parser-length-boundary` or `feature/packed-fragment-fixture`.
- Keep generated files and build directories out of source control.
- Write commits that compile and explain the behavioral change.
- Avoid mixing mass formatting with functional changes.
- Do not add large dependencies, SDKs, or downloads without a documented need and review.

## Code requirements

- Target C++20.
- Preserve row-vector matrix semantics.
- Prefer RAII, scoped enums, value-returning APIs, `std::span`, fixed-width integers, and checked narrowing.
- Validate counts, dimensions, ranges, and multiplication before allocation or mutation.
- Use `override`, `final`, and `noexcept` only when their contracts are correct.
- Reject malformed or unsupported input with the narrowest useful diagnostic category and schema path.
- Do not silently discard recognized data.
- Keep the standalone core independent of Houdini, OpenGL, and OpenVDB runtime linkage.

Formatting follows `.clang-format` and `.editorconfig`. Existing file style should be retained when a full-file reformat is not part of the change.

## Test requirements

At minimum, run the preset relevant to the change. Core changes should normally pass:

```powershell
cmake --preset windows-msvc-werror
cmake --build --preset windows-msvc-werror
ctest --preset windows-msvc-werror

cmake --preset windows-msvc-asan
cmake --build --preset windows-msvc-asan
ctest --preset windows-msvc-asan
```

Linux-sensitive parser or storage work should also pass:

```bash
cmake --preset linux-gcc-ubsan
cmake --build --preset linux-gcc-ubsan
ctest --preset linux-gcc-ubsan
```

Run native MSVC analysis for public API, ownership, indexing, or stream changes:

```powershell
cmake --preset windows-msvc-analysis
cmake --build --preset windows-msvc-analysis
```

Add deterministic tests for every corrected defect and malformed boundary. A test should fail on the old behavior for the reason described by the change.

## Houdini compatibility changes

Follow [Fixture generation and validation](docs/fixtures.md). A compatibility claim requires:

- A minimal generated fixture.
- Exact generation parameters and Houdini build.
- C++ round-trip coverage.
- Houdini-side validation.
- Malformed-input coverage where counts or references are introduced.
- Documentation updates to [Compatibility matrix](docs/compatibility.md).

The maintained Windows matrix currently covers Houdini 20.0.653, 20.5.410, 21.0.631, and 22.0.368.

## Documentation changes

Update documentation when a change affects:

- Public API names or contracts.
- Supported or unsupported records.
- Intentional conversion losses.
- Houdini package behavior.
- Build requirements.
- Experimental or stable format status.
- Release or versioning expectations.

Do not mark roadmap work complete solely from code inspection when executable validation is possible.

## Pull request checklist

Before opening a pull request:

- `git diff --check` is clean.
- Relevant strict and sanitizer tests pass.
- Static analysis passes when applicable.
- New behavior has deterministic regression coverage.
- Unsupported behavior is rejected explicitly.
- Generated files are absent from the diff.
- Documentation and roadmap status are accurate.
- Houdini builds used for validation are named exactly.
- Licensing or provenance implications are recorded.

## Security reports

Do not open a public issue for an undisclosed vulnerability. Follow [Security policy](SECURITY.md).
