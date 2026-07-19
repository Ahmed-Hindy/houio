"""Command-line entry point for HouIO's HOM bridge."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Optional, Sequence

import hou

from .bridge import geometry_to_bgeo_bytes, load_for_houio, save_from_houio


def _geometry_summary(geometry: hou.Geometry) -> dict[str, object]:
    """Return a compact JSON-serializable geometry summary."""
    primitive_types: dict[str, int] = {}
    for primitive in geometry.prims():
        primitive_name = type(primitive).__name__
        primitive_types[primitive_name] = primitive_types.get(primitive_name, 0) + 1
    return {
        "houdini_version": hou.applicationVersionString(),
        "points": len(geometry.points()),
        "vertices": sum(len(primitive.vertices()) for primitive in geometry.prims()),
        "primitives": len(geometry.prims()),
        "primitive_types": primitive_types,
    }


def _decode(input_path: Path, output_path: Path, keep_vdb: bool) -> int:
    """Decode a Houdini-supported file to uncompressed bgeo."""
    geometry = load_for_houio(input_path, convert_vdb=not keep_vdb)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(geometry_to_bgeo_bytes(geometry))
    print(json.dumps(_geometry_summary(geometry), indent=2, sort_keys=True))
    return 0


def _encode(input_path: Path, output_path: Path, keep_volume: bool) -> int:
    """Encode uncompressed bgeo bytes through Houdini's file writers."""
    save_from_houio(
        input_path.read_bytes(),
        output_path,
        convert_volume_for_vdb=not keep_volume,
    )
    geometry = load_for_houio(output_path, convert_vdb=False)
    print(json.dumps(_geometry_summary(geometry), indent=2, sort_keys=True))
    return 0


def _inspect(input_path: Path, convert_vdb: bool) -> int:
    """Inspect a Houdini-supported geometry file."""
    geometry = load_for_houio(input_path, convert_vdb=convert_vdb)
    print(json.dumps(_geometry_summary(geometry), indent=2, sort_keys=True))
    return 0


def _create_parser() -> argparse.ArgumentParser:
    """Create the command-line parser."""
    parser = argparse.ArgumentParser(
        prog="python -m houio_hom",
        description="Convert Houdini geometry files for HouIO using HOM.",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    decode_parser = subparsers.add_parser(
        "decode",
        help="Convert .bgeo.sc or scalar .vdb input to uncompressed .bgeo bytes.",
    )
    decode_parser.add_argument("input", type=Path)
    decode_parser.add_argument("output", type=Path)
    decode_parser.add_argument(
        "--keep-vdb",
        action="store_true",
        help="Do not convert VDB primitives to dense volumes.",
    )

    encode_parser = subparsers.add_parser(
        "encode",
        help="Use Houdini to write HouIO-produced .bgeo bytes as .bgeo.sc or .vdb.",
    )
    encode_parser.add_argument("input", type=Path)
    encode_parser.add_argument("output", type=Path)
    encode_parser.add_argument(
        "--keep-volume",
        action="store_true",
        help="Do not convert dense volumes before .vdb output.",
    )

    inspect_parser = subparsers.add_parser("inspect", help="Print a geometry summary.")
    inspect_parser.add_argument("input", type=Path)
    inspect_parser.add_argument(
        "--convert-vdb",
        action="store_true",
        help="Convert VDB primitives to dense volumes before reporting.",
    )
    return parser


def main(arguments: Optional[Sequence[str]] = None) -> int:
    """Run the HOM bridge command-line interface."""
    parsed = _create_parser().parse_args(arguments)
    if parsed.command == "decode":
        return _decode(parsed.input, parsed.output, parsed.keep_vdb)
    if parsed.command == "encode":
        return _encode(parsed.input, parsed.output, parsed.keep_volume)
    if parsed.command == "inspect":
        return _inspect(parsed.input, parsed.convert_vdb)
    raise AssertionError(f"Unhandled command: {parsed.command}")


if __name__ == "__main__":
    raise SystemExit(main())
