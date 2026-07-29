# Versioning and release policy

HouIO currently uses development version identifiers for private evaluation builds and package archives, but it is not ready for public redistribution. The parent HouIO repository and imported history contain no project-wide license grant. Public release therefore requires written permission from the relevant upstream copyright holders or a documented independently licensable replacement of upstream-derived implementation. See [Source provenance and release status](provenance.md).

The existing `0.x` package version is a development identifier. It does not override the redistribution warning and does not imply that a supported public release has occurred.

## Semantic versioning

Once release blockers are resolved, HouIO will use Semantic Versioning:

- **Major**: incompatible public C++ API, CLI, package, or stable-format changes.
- **Minor**: backward-compatible features, newly supported records, or additional maintained Houdini versions.
- **Patch**: backward-compatible defect fixes, diagnostics, validation, and packaging corrections.

A release candidate may use prerelease identifiers such as `1.0.0-rc.1`.

## Public compatibility surfaces

Versioning applies to maintained, documented surfaces:

- Installed C++ headers and exported CMake target `houio::houio`.
- `GeometryIO` read/write behavior and documented diagnostics.
- `houio_convert` command-line syntax and exit behavior.
- Houdini package entry points and documented bridge behavior.
- Formats explicitly documented as stable HouIO-owned formats.

Reading third-party Houdini formats is compatibility work, not ownership of those formats. Support may expand as representative fixtures are added, but recognized unsupported records must fail explicitly rather than being silently discarded.

## Experimental surfaces

Experimental APIs or formats may change during a minor `0.x` update when the change is documented clearly. The current native `FieldIO` binary layout is experimental and excluded from stable format guarantees. See [Experimental field persistence format](field-format.md).

An experimental surface should be promoted to stable only after it has:

- A written format or API contract.
- Cross-platform or platform-specific scope stated explicitly.
- Compatibility fixtures.
- Malformed-input tests.
- Migration expectations.

## Deprecation policy

After a stable release:

1. A public API planned for removal should first be documented as deprecated.
2. The replacement and migration path should be available in the same release.
3. Removal should normally wait for the next major version.
4. Security fixes may remove unsafe behavior sooner when retaining it would expose users to material risk.

Internal helpers, tests, undocumented implementation details, and experimental surfaces do not receive the same deprecation window.

## Houdini compatibility policy

The maintained matrix records exact tested builds. Supporting a Houdini release line means:

- Generated fixture outputs load and compare successfully.
- The large-asset round trip passes.
- The headless package test passes.

A newer production build in the same Houdini line may work but is not considered validated until it is added to the matrix. Dropping a supported Houdini release line after stable releases requires a major version or an announced support-policy transition.

## Release checklist

A public release must not be created until all of the following are satisfied:

- Written upstream permission or an equivalent legal basis is documented.
- A project-wide license covering the complete combined work is present.
- Retired TTL code is absent and half-float provenance is documented.
- Required source, binary, and legal-status notices are included.
- Strict, sanitizer, static-analysis, fuzzing, package, and Houdini compatibility checks are green.
- Release notes identify support additions, fixes, breaks, and known limitations.
- Binary artifacts include checksums and build provenance.
- CI release jobs are explicitly enabled only after the redistribution blocker is resolved.
- The version and Git tag match the packaged metadata.

## Change log expectations

Release notes should group changes into:

- Added
- Changed
- Fixed
- Deprecated
- Removed
- Security
- Compatibility

Fixture-backed support claims should name the tested Houdini builds and the record or behavior covered.
