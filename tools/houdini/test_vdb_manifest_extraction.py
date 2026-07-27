"""Validate exact live-HOM extraction of a scalar Float VDB manifest."""

from __future__ import annotations

import math

import hou

from houio_hom.manifest import UnsupportedHOMDataError, geometry_manifest


def build_source() -> hou.Geometry:
    """Create a fog Float VDB with two deterministic active voxels."""
    dense = hou.Geometry()
    volume = dense.createVolume(4, 4, 4)
    values = [0.0] * 64
    values[21] = 1.0
    values[22] = 2.0
    volume.setAllVoxels(values)
    verb = hou.sopNodeTypeCategory().nodeVerb("convertvdb")
    if verb is None:
        raise RuntimeError("Convert VDB SOP verb is unavailable")
    verb.setParms({"conversion": 1})
    result = hou.Geometry()
    verb.execute(result, [dense])
    if len(result.prims()) != 1 or not isinstance(result.prims()[0], hou.VDB):
        raise AssertionError("Convert VDB did not produce one VDB primitive")
    return result


def build_ambiguous_source() -> hou.Geometry:
    """Create one active voxel whose value equals the grid background."""
    dense = hou.Geometry()
    volume = dense.createVolume(4, 4, 4)
    volume.setAllVoxels([0.0] * 64)
    convert = hou.sopNodeTypeCategory().nodeVerb("convertvdb")
    activate = hou.sopNodeTypeCategory().nodeVerb("vdbactivate")
    if convert is None or activate is None:
        raise RuntimeError("Required VDB SOP verbs are unavailable")
    convert.setParms({"conversion": 1})
    base = hou.Geometry()
    convert.execute(base, [dense])
    activate.setParms(
        {
            "operation": 0,
            "min": hou.Vector3(1, 1, 1),
            "max": hou.Vector3(1, 1, 1),
            "setvalue": 1,
            "value": 0.0,
            "prune": 0,
        }
    )
    result = hou.Geometry()
    activate.execute(result, [base])
    if result.prims()[0].activeVoxelCount() != 1:
        raise AssertionError("VDB Activate did not create ambiguous activity")
    return result


def main() -> int:
    """Extract one exact VDB and reject one ambiguous active topology."""
    manifest = geometry_manifest(build_source())
    primitive = manifest["primitives"][0]
    if primitive["type"] != "sparse_float_vdb":
        raise AssertionError("Live VDB extraction did not use the sparse manifest type")
    if primitive["grid_class"] != "fog_volume" or primitive["background"] != 0.0:
        raise AssertionError("Live VDB extraction changed class or background")
    if primitive["active_indices"] != [1, 1, 1, 2, 1, 1]:
        raise AssertionError("Live VDB extraction changed active topology")
    if primitive["active_values"] != [1.0, 2.0]:
        raise AssertionError("Live VDB extraction changed active values")
    transform = primitive["index_to_world"]
    if len(transform) != 16 or not all(math.isfinite(value) for value in transform):
        raise AssertionError("Live VDB extraction returned an invalid transform")

    try:
        geometry_manifest(build_ambiguous_source())
    except UnsupportedHOMDataError as error:
        if "exact active topology" not in str(error):
            raise AssertionError("Ambiguous VDB returned the wrong diagnostic") from error
    else:
        raise AssertionError("Ambiguous VDB activity was extracted approximately")

    print("HouIO live VDB manifest extraction passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
