# HouIO command-line interface

`houio` is the primary command-line entry point for the custom writer, inspection, validation, and runtime diagnostics. `houio_convert` remains available as a compatibility two-path converter.

## Commands

```text
houio write <input> <output> [options]
houio write-manifest <manifest.json> <output> [options]
houio convert <input> <output> [options]
houio inspect <input> [--json]
houio validate <input> [--json]
houio capabilities [--json]
houio diagnose [--json]
```

`write` and `convert` read a supported GEO/BGEO/SCF document and serialize it through HouIO's custom writer. `write-manifest` is the direct-HOM bridge entry point for the `houio.hom/1` interchange schema.

## Write options

- `--format <bgeo|bgeo.sc|geo|vdb>` overrides extension-based format selection. ASCII GEO and standalone `.vdb` container output remain unavailable; native VDB primitive records are preserved inside supported GEO/BGEO/SCF documents.
- `--no-overwrite` rejects an existing destination.
- `--no-create-directories` requires the parent directory to exist.
- `--no-atomic` writes the destination directly instead of replacing it from a completed temporary file.
- `--json` emits one machine-readable result object.

## Exit codes

| Code | Meaning |
| ---: | --- |
| `0` | Success |
| `1` | Operation or schema failure |
| `2` | Invalid command-line usage |
| `3` | Input read or parse failure |
| `4` | Output write or replacement failure |
| `5` | Recognized but unsupported format or record |
| `10` | Unexpected internal exception |

Scripts should rely on the exit code for control flow and use JSON diagnostics for details.

## JSON diagnostics

Diagnostics contain:

- `severity`: `warning` or `error`
- `category`: `io`, `malformed_input`, `unsupported_input`, `schema`, or `conversion`
- `message`: human-readable explanation
- `byte_offset`: parser byte offset, or `-1` when unavailable
- `path`: smallest useful file, option, or schema path

Example:

```powershell
$result = houio inspect asset.bgeo.sc --json | ConvertFrom-Json
if (-not $result.success) {
    $result.diagnostics | Format-Table severity, category, path, message
}
```

## Capability reporting

```powershell
houio capabilities
houio capabilities --json
```

A capability is:

- `supported`: the maintained reader/writer path is implemented.
- `recognized`: the family is known but its adapter or fixture-backed schema is incomplete.
- `unavailable`: the feature requires a backend that is not present.

Embedded `PackedGeometry`, named `PackedFragment`, and opaque native VDB payload preservation are supported. Packed disk primitives and OpenVDB-backed sparse-tree construction/editing remain separate additions. `houio inspect` reports separate counts for packed geometry, packed fragments, and native VDB records.

## Runtime diagnostics

```powershell
houio diagnose --json
```

The diagnostic result reports the version, platform, default write format, atomic-replacement availability, and capability count. The Houdini package surfaces the same result through **Package Diagnostics**.
