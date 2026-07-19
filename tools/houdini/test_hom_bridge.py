"""Validate HouIO's HOM bridge in an installed Houdini version."""

from __future__ import annotations

import argparse
from pathlib import Path

import hou

from houio_hom import (
    convert_vdb_to_volume,
    convert_volume_to_vdb,
    convert_with_houio,
    geometry_from_bgeo_bytes,
    geometry_to_bgeo_bytes,
    load_for_houio,
    roundtrip_node_geometry,
    save_from_houio,
)


def build_dense_volume() -> hou.Geometry:
    """Create a deterministic scalar volume for bridge validation."""
    geometry = hou.Geometry()
    bounding_box = hou.BoundingBox(-2.0, -1.0, 3.0, 6.0, 5.0, 7.0)
    volume = geometry.createVolume(17, 2, 1, bounding_box)
    volume.setAllVoxels(tuple(float(x + y * 100) for y in range(2) for x in range(17)))
    return geometry


def volume_summary(
    geometry: hou.Geometry,
) -> tuple[tuple[int, int, int], tuple[float, ...], tuple[float, ...]]:
    """Return comparable resolution, voxel, and transform data."""
    primitives = geometry.prims()
    if len(primitives) != 1 or not isinstance(primitives[0], hou.Volume):
        raise AssertionError("Expected exactly one dense scalar volume")
    volume = primitives[0]
    resolution = tuple(int(value) for value in volume.resolution())
    values = tuple(float(value) for value in volume.allVoxels())
    transform = tuple(
        float(value) for row in volume.transform().asTupleOfTuples() for value in row
    )
    return resolution, values, transform


def validate(output_directory: Path) -> None:
    """Run all HOM bridge round-trips."""
    output_directory.mkdir(parents=True, exist_ok=True)
    source = build_dense_volume()
    source_summary = volume_summary(source)

    bgeo_bytes = geometry_to_bgeo_bytes(source)
    loaded_bytes = geometry_from_bgeo_bytes(bgeo_bytes)
    if volume_summary(loaded_bytes) != source_summary:
        raise AssertionError("Bgeo byte bridge changed the source volume")

    vdb_geometry = convert_volume_to_vdb(source)
    if len(vdb_geometry.prims()) != 1 or not isinstance(
        vdb_geometry.prims()[0], hou.VDB
    ):
        raise AssertionError("Dense volume conversion did not produce a VDB")

    vdb_path = output_directory / "density.vdb"
    vdb_geometry.saveToFile(str(vdb_path))
    loaded_vdb = load_for_houio(vdb_path)
    if volume_summary(loaded_vdb) != source_summary:
        raise AssertionError(
            "VDB to dense-volume conversion changed values or transform"
        )

    scf_path = output_directory / "density.bgeo.sc"
    save_from_houio(bgeo_bytes, scf_path)
    loaded_scf = load_for_houio(scf_path, convert_vdb=False)
    if volume_summary(loaded_scf) != source_summary:
        raise AssertionError("HOM bgeo.sc bridge changed values or transform")

    vdb_from_bytes_path = output_directory / "density_from_houio.vdb"
    save_from_houio(bgeo_bytes, vdb_from_bytes_path)
    converted_back = convert_vdb_to_volume(
        load_for_houio(vdb_from_bytes_path, convert_vdb=False)
    )
    if volume_summary(converted_back) != source_summary:
        raise AssertionError(
            "HouIO bytes to VDB round-trip changed values or transform"
        )

    converter_scf_path = output_directory / "density_converter.bgeo.sc"
    convert_with_houio(vdb_path, converter_scf_path)
    converter_result = load_for_houio(converter_scf_path, convert_vdb=False)
    if volume_summary(converter_result) != source_summary:
        raise AssertionError("HOM subprocess bridge changed values or transform")

    converter_vdb_path = output_directory / "density_converter.vdb"
    convert_with_houio(scf_path, converter_vdb_path)
    converter_vdb_result = load_for_houio(converter_vdb_path)
    if volume_summary(converter_vdb_result) != source_summary:
        raise AssertionError("HouIO subprocess VDB output changed values or transform")

    object_context = hou.node("/obj")
    if object_context is None:
        raise AssertionError("Houdini object context is unavailable")
    geometry_container = object_context.createNode("geo", node_name="houio_hom_test")
    try:
        box_node = geometry_container.createNode("box")
        source_box = box_node.geometry()
        processed_box = roundtrip_node_geometry(box_node)
        if len(processed_box.points()) != len(source_box.points()) or len(
            processed_box.prims()
        ) != len(source_box.prims()):
            raise AssertionError("Cooked SOP round-trip changed box counts")
    finally:
        geometry_container.destroy()


def main() -> int:
    """Run the HOM bridge validation."""
    parser = argparse.ArgumentParser()
    parser.add_argument("output_directory", type=Path)
    arguments = parser.parse_args()
    validate(arguments.output_directory)
    print(f"HOM bridge passed in Houdini {hou.applicationVersionString()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
