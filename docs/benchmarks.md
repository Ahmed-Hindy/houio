# Performance baselines

HouIO includes an opt-in, dependency-free benchmark executable for repeatable local measurements. Benchmarks are not registered as CTest tests and do not enforce timing thresholds in CI because wall-clock performance depends on hardware, power policy, background load, compiler, and allocator behavior.

## Build

From a Visual Studio developer shell:

```powershell
cmake --preset windows-msvc-benchmarks
cmake --build --preset windows-msvc-benchmarks
```

Run the default workload:

```powershell
.\build\windows-msvc-benchmarks\houio_benchmarks.exe
```

Emit machine-readable CSV:

```powershell
.\build\windows-msvc-benchmarks\houio_benchmarks.exe --csv
```

## Workloads

### Numeric attribute write/read

Creates a three-component Float32 `Attribute`, writes every element through the validated typed setter, then reads every element through the typed getter and accumulates a checksum.

Default workload: 250,000 vector elements.

This baseline exercises:

- Attribute allocation and resize.
- Typed representation validation.
- Element offset calculation.
- `memcpy`-based typed writes and reads.

### Triangle-grid generation/traversal

Builds a generated triangle grid and traverses the complete index buffer while accumulating a checksum.

Default workload: 256 by 256 points.

This baseline exercises:

- Position attribute construction.
- Checked grid arithmetic.
- Triangle index generation.
- Contiguous `std::span` index traversal.

### Dense constant-volume import

Generates a deterministic in-memory Houdini Volume document with a constant voxel payload, imports it through `HouGeoIO`, validates the resulting primitive type, and reads opposite corner voxels.

Default workload: 96 cubed voxels.

This baseline exercises:

- ASCII JSON parsing.
- Volume schema validation.
- Checked voxel-count arithmetic.
- Dense field allocation and constant fill.
- Semantic primitive construction.

## Options

```text
--iterations N
--attribute-elements N
--grid-resolution N
--volume-resolution N
--csv
```

Each workload runs independently for the requested number of iterations. The executable reports the median elapsed time, units per second, and a cumulative checksum.

Example reduced smoke run:

```powershell
.\build\windows-msvc-benchmarks\houio_benchmarks.exe `
  --iterations 2 `
  --attribute-elements 10000 `
  --grid-resolution 64 `
  --volume-resolution 32
```

## Comparing results

For useful before/after comparisons:

1. Use the same commit configuration, compiler, build type, and command line.
2. Close unrelated heavy workloads.
3. Keep the machine on a consistent power profile.
4. Run each revision several times and compare medians, not one sample.
5. Record CPU, memory, operating system, compiler, and HouIO commit.
6. Treat changes smaller than ordinary run-to-run variance as inconclusive.

Do not commit machine-specific timing numbers as universal performance claims.

## Scope not yet measured

The current executable does not measure peak memory amplification from compressed input through the JSON tree to `HouGeo`. Standard allocation is not currently routed through an instrumented allocator, and retained process memory is not a reliable portable peak measurement. That roadmap item remains open until allocation tracking or a sampling profiler is integrated deliberately.

The benchmarks also do not yet cover direct semantic handlers, streaming APIs, native sparse volumes, or large mixed-record assets beyond the separate Crag compatibility test.
