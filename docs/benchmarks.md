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

### Input-to-tree-to-HouGeo memory amplification

`houio_memory_probe` generates a deterministic binary point-attribute document, releases the source geometry, then retains these stages simultaneously:

1. The binary input buffer.
2. The parsed `JSONReader` tree.
3. The loaded `HouGeo` semantic model.

Run the default 500,000-point probe:

```powershell
.\build\windows-msvc-benchmarks\houio_memory_probe.exe
```

Emit one CSV row or choose a reduced workload:

```powershell
.\build\windows-msvc-benchmarks\houio_memory_probe.exe `
  --elements 100000 `
  --csv
```

The report includes exact input bytes, incremental current-working-set deltas for the JSON and `HouGeo` stages, combined extra bytes, stage amplification ratios, and a checksum. Working-set sampling is supported on Windows and Linux.

Binary uniform numeric arrays are retained in the generic JSON tree at their encoded widths: Bool remains bit-packed, Int8/UInt8 use one byte, Int16/UInt16/Float16 use two bytes, Int32/Float32 use four bytes, and Int64/Float64 use eight bytes per element. Large homogeneous ordinary numeric arrays are also compacted into contiguous storage when they contain at least 64 scalar elements. Small, mixed, nested, and string arrays retain expanded `Value` storage. Binary tree rewrites and HouGeo payload exports emit compact storage directly without typed temporary vectors; scalar access and ASCII output preserve the established logical values. Numeric attribute loading also copies compact tuple or component arrays directly when their storage token exactly matches the destination, avoiding per-scalar extraction while preserving the conversion fallback for mismatched representations. HouGeo-to-simplified conversion preallocates final `P` and point/vertex `UV` buffers, reuses one topology scratch buffer across polygon faces, and simplified-to-HouGeo V3f position promotion preallocates its final V4f buffer.

The probe deliberately does not define pass/fail thresholds. Current working set is affected by allocator reuse, page commitment, operating-system accounting, background activity, and measurement order. Compare revisions only on the same machine, compiler, build type, workload, and process conditions. Use an allocation profiler when exact allocation ownership or transient peak memory is required.

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

The memory probe samples retained current working set; it does not attribute allocations to individual types and does not capture every transient peak. Exact allocator-level accounting remains future diagnostic work rather than a release gate.

The benchmarks also do not yet cover streaming APIs, native sparse volumes, or large mixed-record assets beyond the separate Crag compatibility test. A full direct semantic handler was evaluated and deferred because the remaining structural-tree cost has not yet justified duplicating the complete flattened HouGeo schema state machine; see [Direct semantic-handler evaluation](direct-semantic-handler.md).
