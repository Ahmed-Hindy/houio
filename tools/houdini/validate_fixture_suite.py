"""Validate minimal HouIO fixture round-trips through Houdini."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

import hou

ATTRIBUTE_DOMAINS = ("point", "vertex", "primitive", "global")


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments.

    Returns:
        Parsed command-line arguments.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path, help="Generated fixture manifest")
    parser.add_argument("source_directory", type=Path, help="Directory containing source fixtures")
    parser.add_argument("candidate_directory", type=Path, help="Directory containing HouIO outputs")
    return parser.parse_args()


def load_geometry(path: Path) -> hou.Geometry:
    """Load a geometry file into a frozen Houdini geometry object.

    Args:
        path: Geometry file to load.

    Returns:
        Loaded geometry.

    Raises:
        FileNotFoundError: If the file does not exist.
        RuntimeError: If Houdini cannot load the file.
    """
    resolved_path = path.resolve()
    if not resolved_path.is_file():
        raise FileNotFoundError(resolved_path)

    geometry = hou.Geometry()
    try:
        geometry.loadFromFile(str(resolved_path))
    except hou.OperationFailed as error:
        raise RuntimeError(f"Houdini failed to load {resolved_path}: {error}") from error
    return geometry.freeze()


def domain_attributes(geometry: hou.Geometry, domain: str) -> tuple[hou.Attrib, ...]:
    """Return attributes for one Houdini owner domain.

    Args:
        geometry: Geometry containing the attributes.
        domain: Point, vertex, primitive, or global.

    Returns:
        Attributes in the requested domain.

    Raises:
        ValueError: If the domain is unsupported.
    """
    if domain == "point":
        return geometry.pointAttribs()
    if domain == "vertex":
        return geometry.vertexAttribs()
    if domain == "primitive":
        return geometry.primAttribs()
    if domain == "global":
        return geometry.globalAttribs()
    raise ValueError(f"Unsupported attribute domain: {domain}")


def flatten_value(value: Any) -> list[Any]:
    """Normalize a scalar or tuple attribute value into a flat list.

    Args:
        value: Houdini attribute value.

    Returns:
        Flat list representation.
    """
    if isinstance(value, (tuple, list)):
        return list(value)
    return [value]


def attribute_values(geometry: hou.Geometry, domain: str, attribute: hou.Attrib) -> list[Any]:
    """Read flat values for one supported Houdini attribute.

    Args:
        geometry: Geometry containing the attribute.
        domain: Attribute owner domain.
        attribute: Attribute to read.

    Returns:
        Flat attribute values.

    Raises:
        RuntimeError: If the attribute data type is unsupported.
    """
    attribute_name = attribute.name()
    if domain == "global":
        return flatten_value(geometry.attribValue(attribute_name))

    value_readers = {
        ("point", hou.attribData.Float): geometry.pointFloatAttribValues,
        ("point", hou.attribData.Int): geometry.pointIntAttribValues,
        ("point", hou.attribData.String): geometry.pointStringAttribValues,
        ("vertex", hou.attribData.Float): geometry.vertexFloatAttribValues,
        ("vertex", hou.attribData.Int): geometry.vertexIntAttribValues,
        ("vertex", hou.attribData.String): geometry.vertexStringAttribValues,
        ("primitive", hou.attribData.Float): geometry.primFloatAttribValues,
        ("primitive", hou.attribData.Int): geometry.primIntAttribValues,
        ("primitive", hou.attribData.String): geometry.primStringAttribValues,
    }
    reader = value_readers.get((domain, attribute.dataType()))
    if reader is None:
        raise RuntimeError(
            f"Unsupported {domain} attribute type for {attribute_name!r}: {attribute.dataType()}"
        )
    return list(reader(attribute_name))


def attribute_summary(geometry: hou.Geometry, domain: str) -> dict[str, dict[str, Any]]:
    """Summarize all attributes in one owner domain.

    Args:
        geometry: Geometry to summarize.
        domain: Attribute owner domain.

    Returns:
        Attribute metadata and flat values keyed by name.
    """
    summary = {}
    for attribute in domain_attributes(geometry, domain):
        summary[attribute.name()] = {
            "data_type": str(attribute.dataType()),
            "size": attribute.size(),
            "values": attribute_values(geometry, domain, attribute),
        }
    return summary


def primitive_summary(geometry: hou.Geometry) -> list[dict[str, Any]]:
    """Summarize supported primitive data in primitive order.

    Args:
        geometry: Geometry to summarize.

    Returns:
        Polygon topology or dense-volume values and transforms.
    """
    summaries = []
    for primitive in geometry.prims():
        if isinstance(primitive, hou.Volume):
            summaries.append(
                {
                    "type": primitive.type().name(),
                    "closed": None,
                    "points": [vertex.point().number() for vertex in primitive.vertices()],
                    "resolution": list(primitive.resolution()),
                    "transform": list(primitive.transform().asTuple()),
                    "position": list(primitive.vertex(0).point().position()),
                    "voxels": list(primitive.allVoxels()),
                }
            )
            continue

        summaries.append(
            {
                "type": primitive.type().name(),
                "closed": primitive.isClosed(),
                "points": [vertex.point().number() for vertex in primitive.vertices()],
            }
        )
    return summaries


def geometry_summary(geometry: hou.Geometry) -> dict[str, Any]:
    """Create a complete deterministic comparison summary.

    Args:
        geometry: Geometry to summarize.

    Returns:
        Counts, attributes, topology, and all group domains.
    """
    return {
        "point_count": len(geometry.points()),
        "vertex_count": sum(len(primitive.vertices()) for primitive in geometry.prims()),
        "primitive_count": len(geometry.prims()),
        "attributes": {
            domain: attribute_summary(geometry, domain) for domain in ATTRIBUTE_DOMAINS
        },
        "primitives": primitive_summary(geometry),
        "point_groups": {
            group.name(): [point.number() for point in group.points()]
            for group in geometry.pointGroups()
        },
        "vertex_groups": {
            group.name(): [vertex.number() for vertex in group.vertices()]
            for group in geometry.vertexGroups()
        },
        "primitive_groups": {
            group.name(): [primitive.number() for primitive in group.prims()]
            for group in geometry.primGroups()
        },
    }


def validate_manifest_source(fixture_name: str, expected: dict[str, Any], actual: dict[str, Any]) -> None:
    """Ensure generated source geometry still matches its manifest.

    Args:
        fixture_name: Fixture identifier.
        expected: Manifest source summary.
        actual: Loaded source summary.

    Raises:
        RuntimeError: If the generated fixture does not match the manifest.
    """
    for count_name in ("point_count", "vertex_count", "primitive_count"):
        if actual[count_name] != expected[count_name]:
            raise RuntimeError(
                f"{fixture_name}: source {count_name} changed: "
                f"{actual[count_name]} != {expected[count_name]}"
            )

    manifest_attribute_keys = {
        "point": "point_attributes",
        "vertex": "vertex_attributes",
        "primitive": "primitive_attributes",
        "global": "global_attributes",
    }
    for domain, manifest_key in manifest_attribute_keys.items():
        actual_names = sorted(actual["attributes"][domain])
        if actual_names != expected[manifest_key]:
            raise RuntimeError(
                f"{fixture_name}: source {domain} attributes changed: "
                f"{actual_names} != {expected[manifest_key]}"
            )

    actual_closed = [primitive["closed"] for primitive in actual["primitives"]]
    if actual_closed != expected["closed"]:
        raise RuntimeError(
            f"{fixture_name}: source closed state changed: {actual_closed} != {expected['closed']}"
        )

    for group_key in ("point_groups", "vertex_groups", "primitive_groups"):
        actual_group_names = sorted(actual[group_key])
        if actual_group_names != expected[group_key]:
            raise RuntimeError(
                f"{fixture_name}: source {group_key} changed: "
                f"{actual_group_names} != {expected[group_key]}"
            )


def compare_roundtrip(fixture_name: str, source: dict[str, Any], candidate: dict[str, Any], known_losses: set[str]) -> None:
    """Compare one source and HouIO candidate summary.

    Args:
        fixture_name: Fixture identifier.
        source: Source geometry summary.
        candidate: Candidate geometry summary.
        known_losses: Explicitly documented unsupported features.

    Raises:
        RuntimeError: If an unexpected difference is found.
    """
    for key in ("point_count", "vertex_count", "primitive_count", "attributes", "primitives"):
        if candidate[key] != source[key]:
            raise RuntimeError(
                f"{fixture_name}: round-trip mismatch for {key}:\n"
                f"source={source[key]!r}\n"
                f"candidate={candidate[key]!r}"
            )

    for group_key in ("point_groups", "vertex_groups", "primitive_groups"):
        if group_key in known_losses:
            if candidate[group_key]:
                raise RuntimeError(
                    f"{fixture_name}: {group_key} are unsupported but candidate contained "
                    f"{candidate[group_key]!r}"
                )
        elif candidate[group_key] != source[group_key]:
            raise RuntimeError(
                f"{fixture_name}: {group_key} mismatch: "
                f"{candidate[group_key]!r} != {source[group_key]!r}"
            )


def main() -> int:
    """Validate every generated fixture and HouIO output.

    Returns:
        Process exit code.
    """
    args = parse_args()
    manifest_path = args.manifest.resolve()
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    source_directory = args.source_directory.resolve()
    candidate_directory = args.candidate_directory.resolve()

    fixture_results = []
    for fixture in manifest["fixtures"]:
        fixture_name = fixture["name"]
        source_path = source_directory / fixture["file"]
        candidate_path = candidate_directory / fixture["file"]
        source_summary = geometry_summary(load_geometry(source_path))
        candidate_summary = geometry_summary(load_geometry(candidate_path))
        known_losses = set(fixture["known_losses"])

        validate_manifest_source(fixture_name, fixture["source"], source_summary)
        compare_roundtrip(fixture_name, source_summary, candidate_summary, known_losses)
        fixture_results.append(
            {
                "name": fixture_name,
                "known_losses": sorted(known_losses),
                "point_count": source_summary["point_count"],
                "vertex_count": source_summary["vertex_count"],
                "primitive_count": source_summary["primitive_count"],
            }
        )

    summary = {
        "houdini_version": hou.applicationVersionString(),
        "manifest": str(manifest_path),
        "fixture_count": len(fixture_results),
        "fixtures": fixture_results,
    }
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
