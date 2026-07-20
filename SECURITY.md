# Security Policy

HouIO is an experimental file-format library. It parses complex binary and ASCII geometry supplied by callers, so malformed-input reports are treated as security-relevant even when they only reproduce in development builds.

## Supported versions

There are no supported binary releases yet. Security fixes are developed against the current `master` branch. Older commits and downstream forks may not receive fixes.

## Reporting a vulnerability

Do not publish a crashing file, exploit details, or sensitive production geometry in a public issue.

Use GitHub's private vulnerability-reporting flow from the repository **Security** tab when it is available. If private reporting is unavailable, open a public issue containing only a request for a private contact channel and no reproduction data.

Include as much of the following as possible in the private report:

- The affected commit SHA.
- Operating system and compiler.
- Build preset and sanitizer configuration.
- File type, such as `.geo`, `.bgeo`, `.bgeo.sc`, or HOM-assisted `.vdb`.
- The smallest non-sensitive reproducer you can provide.
- Exact command line or API call.
- Stack trace and sanitizer output.
- Whether configured parser or decompression limits were changed.

A minimized synthetic file is preferable to production geometry. Remove proprietary names and attributes before sharing data.

## Relevant issue classes

Examples include:

- Out-of-bounds reads or writes.
- Use-after-free or invalid lifetime behavior.
- Integer overflow that bypasses allocation or payload limits.
- Stack exhaustion or uncontrolled recursion.
- Unbounded allocation or decompression amplification.
- Parser state corruption caused by malformed tokens.
- Crashes or exceptions escaping diagnostics-aware APIs.
- Unsafe dynamic-library or subprocess resolution in SCF or HOM workflows.

Ordinary unsupported primitive diagnostics and documented lossy conversions are not security vulnerabilities unless they cause memory corruption, uncontrolled resource use, or unexpected code execution.

## Reproducing safely

Use the existing sanitizer configurations before reducing a report:

```powershell
cmake --preset windows-msvc-asan
cmake --build --preset windows-msvc-asan
ctest --preset windows-msvc-asan
```

```bash
cmake --preset linux-gcc-ubsan
cmake --build --preset linux-gcc-ubsan
ctest --preset linux-gcc-ubsan
```

The deterministic parser mutation test is registered as `houio.parser_corpus`. Clang builds can also run the optional libFuzzer target:

```bash
cmake --preset linux-clang-fuzzer
cmake --build --preset linux-clang-fuzzer
./build/linux-clang-fuzzer/houio_fuzz_parser -runs=2000 -max_len=512 -timeout=5
```

## Current security boundary

HouIO has parser, string, array, nesting, file-size, SCF decompression, and topology limits, but it is not yet presented as a hardened sandbox for hostile files. Applications processing untrusted data should use conservative limits, isolation appropriate to their threat model, and current sanitizer-tested builds.

The Houdini HOM bridge starts the configured `houio_convert` executable without a shell and applies a finite default timeout. VDB conversion may expand sparse grids into dense memory, so callers must bound grid size before using that workflow.

Security reports are handled on a best-effort basis. No response-time or release-time service level is currently offered.
