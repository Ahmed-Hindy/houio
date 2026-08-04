# Direct semantic-handler evaluation

## Decision

HouIO will not add a second, full HouGeo schema decoder directly on `json::Handler` at this stage.

The maintained import path remains:

1. `json::Parser` validates and emits parser events.
2. `json::JSONReader` builds the generic JSON tree.
3. `HouGeo::toObject` validates flattened key/value records.
4. `HouGeo::load` performs schema-aware semantic decoding with path-qualified diagnostics.

Large numeric payloads no longer require expanded per-element tree storage. Binary uniform arrays retain exact encoded bytes, large homogeneous ordinary arrays compact at array close, and matching numeric attribute storage is copied directly into semantic `Attribute` buffers. This leaves structural records, strings, mixed arrays, primitive metadata, and deliberately retained payloads as the principal tree cost.

## Alternatives considered

### Full direct HouGeo handler

A direct handler could theoretically build `HouGeo` while parser events arrive. It was rejected for the current beta because Houdini geometry uses nested flattened arrays rather than ordinary JSON objects alone. A complete implementation would need to duplicate or replace all of the following behavior:

- Root count and domain validation.
- Flattened key/value-array validation and duplicate-key rejection.
- Attribute definitions, storage conversion, paging, packing, constant pages, indexed strings, and dictionaries.
- Topology validation before primitive construction.
- Shared embedded geometry and shared voxel records.
- Primitive-family dispatch, including packed records, curves, quadrics, volumes, and native VDB payloads.
- Group membership decoding.
- Recursive embedded-geometry imports.
- Existing schema-path diagnostics and exception normalization.

The parser handler contract provides begin/end and scalar/uniform-array events. It does not provide a HouGeo-specific record boundary or field-order contract. A direct implementation would therefore require a second nested schema state machine with broad compatibility risk.

### Partial direct handler for numeric attributes

A handler specialized only for attribute payloads would still need enough surrounding schema state to know the domain, element count, tuple size, destination storage, paging metadata, and current field path. The current compact-tree path already removes retained scalar amplification and directly copies matching storage, while preserving one schema decoder. A separate partial handler would add synchronization burden for a narrower incremental gain.

### Streaming or chunked public import API

A public streaming API is deferred. Primitive references, shared data, root counts, arbitrary field order, recursive geometry, and full-file validation make partial semantic publication difficult to define safely. A streaming contract should not be introduced until ordering, ownership, cancellation, and partial-failure semantics are explicit and fixture-backed.

## Evidence and current mitigations

The current architecture already addresses the dominant numeric-payload costs:

- Exact-width binary uniform storage, including bit-packed Bool and Float16.
- Homogeneous ordinary numeric-array compaction after 64 elements.
- Direct binary rewrite of retained compact arrays.
- Direct contiguous or strided semantic attribute copies when source and destination storage match.
- A Windows/Linux working-set probe covering retained input, JSON tree, and HouGeo stages.

These changes preserve a single schema decoder and the existing diagnostic model.

## Revisit criteria

Reconsider a direct semantic handler only when all of the following are available:

1. Allocation profiling on representative production assets shows structural JSON-tree retention, rather than numeric payloads, is a material remaining bottleneck.
2. A documented input-order and forward-reference policy exists for every maintained HouGeo record family.
3. The direct path can reuse the same schema-validation routines rather than duplicate them.
4. Differential tests compare tree and direct imports across the full fixture corpus, malformed corpus, Crag asset, native VDB payloads, recursive embedded geometry, and all supported Houdini versions.
5. Diagnostics, parser limits, cancellation behavior, and ownership of partially built geometry are specified.
6. The measured gain justifies maintaining two import paths or replacing the existing path outright.

Until those criteria are met, optimization should continue inside the single validated pipeline, especially by reducing copies between retained tree storage, HouGeo storage, adapters, and simplified geometry.
