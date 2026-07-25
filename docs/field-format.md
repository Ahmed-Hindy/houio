# Experimental field persistence format

`include/houio/FieldIO.h` exposes opt-in binary persistence for dense `Field<T>` values. The API is public because it is installed with the other headers, but the on-disk layout is **experimental** and is not a stable interchange format.

Use GEO/BGEO/SCF for maintained Houdini interchange. Use `FieldIO` only for controlled applications that own both the writer and reader and can migrate stored data when the implementation changes.

## Stability and portability

The current format has:

- No magic bytes.
- No format-version field.
- No byte-order marker.
- No checksum.
- No compression.
- Native C++ integer and floating-point representation.
- Raw native layout for vector value types.

Files are therefore not promised to be portable across architectures, endianness, compilers, or future HouIO versions. Semantic versioning does not protect this experimental layout until a versioned format is designed and explicitly promoted to stable status.

## Public functions

```cpp
houio::loadField<T>(filename);
houio::storeField(field, filename);
houio::storeFieldWithoutBoundingBox(field, filename);
```

`loadField<T>` reads the bounded layout only. It validates the type code, dimensions, multiplication bounds, stream-size bounds, and available payload bytes before allocating field storage.

Both writers return `false` for file-open, write, flush, or close failure. Size overflow is reported with `std::length_error`.

## Bounded layout

`storeField` writes the following native values in order:

| Field | Native type | Count |
| --- | --- | --- |
| Resolution x, y, z | `int` | 3 |
| Bound minimum x, y, z | `float` | 3 |
| Bound maximum x, y, z | `float` | 3 |
| Value-type code | `int` | 1 |
| Voxel payload | `T` | `x * y * z` |

Payload order is x-major inside y inside z:

```text
index = z * resolution.x * resolution.y
      + y * resolution.x
      + x
```

## Compact layout

`storeFieldWithoutBoundingBox` writes:

| Field | Native type | Count |
| --- | --- | --- |
| Resolution x, y, z | `int` | 3 |
| Value-type code | `int` | 1 |
| Voxel payload | `T` | `x * y * z` |

The compact layout is retained for legacy/export use and has no matching public loader because it contains no spatial bound or transform. Callers should not use it as a general persistence format.

## Value-type codes

| Code | C++ value type |
| --- | --- |
| 1 | `float` |
| 2 | `houio::math::V3f` |
| 3 | `double` |
| 4 | `houio::math::V3d` |

A reader instantiated with the wrong `T` returns `nullptr`.

## Field coordinate model

A field's local domain is the normalized cube `[0, 1]^3`. Voxel coordinates span `[0, resolution]`; voxel centers are at integer coordinates plus `0.5`. Transform composition follows HouIO's row-vector convention:

```text
voxel -> local -> world
```

The bounded format stores the axis-aligned world bound, not an arbitrary rotated or sheared local-to-world matrix. Loading reconstructs the scale-and-translation transform implied by that bound. Do not use this format when the complete affine transform must survive persistence.

## Future stable-format requirements

A stable successor should define at least:

- Magic and format version.
- Fixed-width integer fields.
- Canonical byte order.
- Explicit scalar/vector encoding.
- Full affine-transform representation.
- Payload length and checksum.
- Optional compression identifier.
- Compatibility and migration rules.

Until those requirements are implemented and tested, the current field format remains experimental.
