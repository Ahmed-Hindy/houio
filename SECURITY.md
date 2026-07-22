# Security policy

HouIO parses caller-supplied ASCII, binary, and compressed geometry. Malformed-input failures are treated as security-relevant when they can cause memory corruption, uncontrolled resource use, parser state corruption, or unsafe process/library resolution.

## Supported code

Security fixes target the current `master` branch. No binary release channel is currently maintained.

## Reporting a vulnerability

Do not publish crashing files, exploit details, or sensitive production geometry in a public issue.

Use the private vulnerability-reporting flow in the GitHub **Security** tab. When private reporting is unavailable, open a public issue requesting a private contact channel without including reproduction data.

Include:

- Affected commit SHA
- Operating system and compiler
- Build preset and sanitizer configuration
- File type: `.geo`, `.bgeo`, `.bgeo.sc`, or bridge-assisted `.vdb`
- Smallest non-sensitive reproducer
- Exact command or API call
- Stack trace and sanitizer output
- Any parser, file-size, decompression, or timeout limits that were changed

Prefer minimized synthetic geometry. Remove proprietary names, paths, and attributes before sharing data.

## Relevant issue classes

Examples:

- Out-of-bounds reads or writes
- Use-after-free or invalid lifetime behavior
- Integer overflow that bypasses limits
- Stack exhaustion or uncontrolled recursion
- Unbounded allocation or decompression amplification
- Parser state corruption
- Exceptions escaping diagnostics-aware APIs
- Unsafe dynamic-library resolution
- Unsafe subprocess resolution or argument handling

A clean unsupported-data diagnostic is not a vulnerability unless it triggers memory corruption, uncontrolled resource use, or unexpected code execution.

## Reproducing safely

Windows AddressSanitizer:

```powershell
cmake --preset windows-msvc-asan
cmake --build --preset windows-msvc-asan
ctest --preset windows-msvc-asan
```

Linux UndefinedBehaviorSanitizer:

```bash
cmake --preset linux-gcc-ubsan
cmake --build --preset linux-gcc-ubsan
ctest --preset linux-gcc-ubsan
```

Clang libFuzzer:

```bash
cmake --preset linux-clang-fuzzer
cmake --build --preset linux-clang-fuzzer
./build/linux-clang-fuzzer/houio_fuzz_parser -runs=2000 -max_len=512 -timeout=5
```

The deterministic mutation test is registered as `houio.parser_corpus`.

## Security boundary

HouIO enforces limits for file size, strings, arrays, nesting, SCF decompression, topology, and primitive ranges. It is not presented as a hardened sandbox for hostile files.

Applications processing untrusted data should:

- Use conservative limits
- Run current sanitizer-tested builds
- Isolate conversion according to their threat model
- Set an explicit C-Blosc library path
- Set finite subprocess timeouts
- Bound VDB resolution before densification

The Houdini bridge starts the configured converter directly without a shell. VDB conversion can expand sparse grids into dense memory and must be constrained by the caller.
