# Source provenance and release status

## Repository lineage

This repository is a fork of `dkoerner/houio`:

```text
https://github.com/dkoerner/houio
```

The imported Git history begins in 2015 and includes work by David Koerner, Nicholas Yue, Nicolas Rondaud, and later contributors to this fork. Neither the parent repository nor the imported history contains a project-wide license file.

A public Git repository without a license is not equivalent to an open-source license grant. The current fork therefore remains unsuitable for public source or binary redistribution unless the required permission is obtained from the relevant upstream copyright holders.

## Current source categories

### Upstream-derived HouIO implementation

The parser, geometry model, file-format implementation, math code, and other portions retain lineage from the original HouIO repository even where they have since been modernized substantially. Modernization commits do not by themselves create a new license grant for those inherited portions.

### Fork-authored implementation

The current fork adds substantial new work, including modern C++ APIs, diagnostics, safety limits, packed primitive families, sparse-grid support, native Houdini integration, Alembic/USD writers, packaging, and validation infrastructure. The authors of those additions can license their own contributions, but cannot unilaterally license upstream-derived code owned by others.

### Retired TTL utility tree

The historical `include/ttl` utility library was replaced by standard-library code and then reduced to compile-time retirement stubs. The entire retired tree has now been deleted. No TTL headers are installed, compiled, or packaged.

The deletion removes a dormant provenance question from the current source tree. It does not change the licensing status of the remaining upstream-derived HouIO code.

### Half-precision conversion

The upstream history contained the ILM/OpenEXR `half` implementation under `include/houio/math/Half` and `src/math/Half`. Those files are no longer compiled and have now been removed from the working tree.

The active `include/houio/HalfFloat.h` implementation was introduced in this fork as a compact bit-conversion utility. The historical ILM/OpenEXR BSD notice is retained conservatively in `THIRD_PARTY_NOTICES.md` because the active utility replaces the same functionality and the original implementation remains visible in Git history.

### Bundled scene dependencies

OpenUSD, Alembic, Imath, and oneTBB are fetched from pinned upstream revisions. The dependency builder copies their notices and creates a manifest containing versions, exact source commits, toolchain metadata, and runtime DLL SHA-256 values. Generated packages place these records under:

```text
share/houio/licenses
```

These dependency licenses cover only their respective components and do not license HouIO as a combined work.

## Distribution policy

Until upstream permission or an equivalent legal basis is documented:

- CI may build and validate private evaluation packages.
- CI must not upload package archives as release artifacts.
- Version tags must not trigger a public binary release workflow.
- Documentation and generated packages must include `LICENSE_STATUS.md`, this provenance record, and `THIRD_PARTY_NOTICES.md`.
- Release notes must not describe a build as open source, redistributable, or publicly released.

## Resolution paths

The release blocker can be resolved by either:

1. Obtaining a written compatible license grant from the relevant upstream copyright holders; or
2. Replacing upstream-derived implementation through a documented independently authored rewrite, followed by a contributor and provenance audit.

Any obtained permission should be stored in a durable project record, and the selected project license should be added only after its scope has been confirmed.
