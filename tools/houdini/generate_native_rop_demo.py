"""Generate an explorable Houdini scene for the native HouIO Geometry ROP."""

from __future__ import annotations

import argparse
from pathlib import Path

import hou


def _set_parm(node: hou.Node, name: str, value) -> None:
    """Set a parameter when it exists on the current Houdini version."""
    parm = node.parm(name)
    if parm is not None:
        parm.set(value)


def _set_expression(node: hou.Node, name: str, expression: str) -> None:
    """Set an HScript expression when the target parameter exists."""
    parm = node.parm(name)
    if parm is not None:
        parm.setExpression(expression, language=hou.exprLanguage.Hscript)


def _decorate(
    node: hou.Node,
    comment: str,
    color: tuple[float, float, float],
    position: tuple[float, float],
) -> None:
    """Apply consistent presentation metadata to a node."""
    node.setComment(comment)
    node.setGenericFlag(hou.nodeFlag.DisplayComment, True)
    node.setColor(hou.Color(color))
    node.setPosition(hou.Vector2(position))


def _clear_default_children(container: hou.Node) -> None:
    """Remove the default File SOP from a newly created Geometry object."""
    for child in container.children():
        child.destroy()


def _build_supported_source(object_context: hou.Node) -> hou.Node:
    """Create an animated polygon and open-polyline source network."""
    container = object_context.createNode("geo", "HOUIO_NATIVE_ROP_SUPPORTED")
    _clear_default_children(container)
    container.setPosition(hou.Vector2((-3.5, 1.5)))
    container.setColor(hou.Color((0.20, 0.55, 0.25)))
    container.setComment(
        "SUPPORTED SOURCE\n"
        "Animated polygon box plus an open polyline.\n"
        "The native ROP currently preserves P, topology, and closure."
    )
    container.setGenericFlag(hou.nodeFlag.DisplayComment, True)

    box = container.createNode("box", "polygon_box")
    _set_parm(box, "type", "poly")
    _set_parm(box, "sizex", 1.8)
    _set_parm(box, "sizey", 1.2)
    _set_parm(box, "sizez", 1.4)
    _decorate(
        box,
        "Closed polygon source",
        (0.25, 0.60, 0.30),
        (-4.0, 1.0),
    )

    animated_box = container.createNode("xform", "animated_box")
    animated_box.setInput(0, box)
    _set_expression(animated_box, "ry", "$F * 7")
    _set_expression(animated_box, "tx", "sin($F * 0.3) * 0.75")
    _set_expression(animated_box, "ty", "cos($F * 0.2) * 0.20")
    _decorate(
        animated_box,
        "Animated every frame\nry = $F * 7",
        (0.35, 0.70, 0.35),
        (-4.0, -0.5),
    )

    line = container.createNode("line", "open_polyline")
    _set_parm(line, "points", 7)
    _set_parm(line, "originx", -1.5)
    _set_parm(line, "originy", -1.0)
    _set_parm(line, "originz", 0.0)
    _set_parm(line, "dirx", 1.0)
    _set_parm(line, "diry", 0.35)
    _set_parm(line, "dirz", 0.25)
    _set_parm(line, "dist", 3.0)
    _decorate(
        line,
        "Open polyline source",
        (0.25, 0.55, 0.80),
        (0.0, 1.0),
    )

    animated_line = container.createNode("xform", "animated_line")
    animated_line.setInput(0, line)
    _set_expression(animated_line, "tz", "sin($F * 0.25) * 0.5")
    _set_expression(animated_line, "rz", "$F * -2")
    _decorate(
        animated_line,
        "Animated open primitive",
        (0.35, 0.65, 0.90),
        (0.0, -0.5),
    )

    merge = container.createNode("merge", "merge_supported_geometry")
    merge.setInput(0, animated_box)
    merge.setInput(1, animated_line)
    _decorate(
        merge,
        "Combined supported geometry",
        (0.65, 0.65, 0.25),
        (-2.0, -2.0),
    )

    output = container.createNode("null", "OUT_SUPPORTED")
    output.setInput(0, merge)
    output.setDisplayFlag(True)
    output.setRenderFlag(True)
    _decorate(
        output,
        "ROP source path\nUse this node for successful exports",
        (0.20, 0.80, 0.35),
        (-2.0, -3.5),
    )

    return output


def _build_unsupported_source(object_context: hou.Node) -> hou.Node:
    """Create a native sphere that demonstrates explicit failure behavior."""
    container = object_context.createNode("geo", "HOUIO_NATIVE_ROP_UNSUPPORTED")
    _clear_default_children(container)
    container.setPosition(hou.Vector2((2.5, 1.5)))
    container.setColor(hou.Color((0.70, 0.22, 0.18)))
    container.setComment(
        "UNSUPPORTED SOURCE\n"
        "A native primitive sphere. The first ROP vertical slice rejects it\n"
        "with an actionable error and does not leave a partial file."
    )
    container.setGenericFlag(hou.nodeFlag.DisplayComment, True)

    sphere = container.createNode("sphere", "native_primitive_sphere")
    _set_parm(sphere, "type", "prim")
    _set_parm(sphere, "radx", 1.25)
    _set_parm(sphere, "rady", 1.25)
    _set_parm(sphere, "radz", 1.25)
    _decorate(
        sphere,
        "Intentional failure case\nNative Sphere primitive",
        (0.85, 0.25, 0.20),
        (0.0, 0.5),
    )

    output = container.createNode("null", "OUT_UNSUPPORTED")
    output.setInput(0, sphere)
    output.setDisplayFlag(True)
    output.setRenderFlag(True)
    _decorate(
        output,
        "Use with HOUIO_EXPORT_UNSUPPORTED\nExpected to fail safely",
        (0.85, 0.35, 0.25),
        (0.0, -1.0),
    )
    return output


def _build_readback(object_context: hou.Node) -> hou.Node:
    """Create a file-SOP network that reads the supported cache sequence."""
    container = object_context.createNode("geo", "HOUIO_RENDERED_READBACK")
    _clear_default_children(container)
    container.setPosition(hou.Vector2((-0.5, -1.5)))
    container.setColor(hou.Color((0.22, 0.48, 0.72)))
    container.setComment(
        "RENDERED CACHE READBACK\n"
        "After rendering HOUIO_EXPORT_SUPPORTED, enable this object's display\n"
        "or inspect its File SOP while scrubbing frames 1-24."
    )
    container.setGenericFlag(hou.nodeFlag.DisplayComment, True)

    file_node = container.createNode("file", "read_houio_cache")
    _set_parm(file_node, "file", "$HIP/cache/houio_native_demo.$F4.bgeo.sc")
    _decorate(
        file_node,
        "Reads the HouIO-generated frame sequence",
        (0.25, 0.55, 0.85),
        (0.0, 0.0),
    )

    output = container.createNode("null", "OUT_READBACK")
    output.setInput(0, file_node)
    output.setDisplayFlag(False)
    output.setRenderFlag(True)
    _decorate(
        output,
        "Toggle display after rendering the supported ROP",
        (0.30, 0.65, 0.95),
        (0.0, -1.5),
    )
    return output


def _configure_frame_range(rop: hou.Node) -> None:
    """Configure a standard 1-24 frame-range render."""
    _set_parm(rop, "trange", 1)
    frame_tuple = rop.parmTuple("f")
    if frame_tuple is not None:
        frame_tuple.set((1.0, 24.0, 1.0))


def _build_rop(
    output_context: hou.Node,
    name: str,
    source: hou.Node,
    output_path: str,
    comment: str,
    color: tuple[float, float, float],
    position: tuple[float, float],
) -> hou.Node:
    """Create and configure one native HouIO Geometry ROP."""
    rop = output_context.createNode("houio::geometry", name)
    _set_parm(rop, "soppath", source.path())
    _set_parm(rop, "sopoutput", output_path)
    _set_parm(rop, "createdirs", 1)
    _set_parm(rop, "overwrite", 1)
    _set_parm(rop, "atomic", 1)
    _configure_frame_range(rop)
    _decorate(rop, comment, color, position)
    return rop


def generate(output_path: Path) -> None:
    """Build and save the complete native ROP exploration scene."""
    if "houio::geometry" not in hou.ropNodeTypeCategory().nodeTypes():
        raise RuntimeError(
            "The native houio::geometry ROP is not loaded. Set HOUDINI_DSO_PATH "
            "to the version-matched ROP_HouIO DSO directory before running this script."
        )

    hou.hipFile.clear(suppress_save_prompt=True)
    object_context = hou.node("/obj")
    output_context = hou.node("/out")
    if object_context is None or output_context is None:
        raise RuntimeError("Houdini object or output context is unavailable")

    supported = _build_supported_source(object_context)
    unsupported = _build_unsupported_source(object_context)
    _build_readback(object_context)

    _build_rop(
        output_context,
        "HOUIO_EXPORT_SUPPORTED",
        supported,
        "$HIP/cache/houio_native_demo.$F4.bgeo.sc",
        "SUCCESS PATH\nRender frames 1-24\nWrites $HIP/cache/houio_native_demo.$F4.bgeo.sc",
        (0.20, 0.75, 0.30),
        (-2.5, 0.5),
    )
    _build_rop(
        output_context,
        "HOUIO_EXPORT_UNSUPPORTED",
        unsupported,
        "$HIP/cache/unsupported_should_not_exist.bgeo",
        "FAILURE PATH\nExpected actionable error\nNo partial output should remain",
        (0.85, 0.25, 0.20),
        (2.5, 0.5),
    )

    hou.playbar.setFrameRange(1, 24)
    hou.playbar.setPlaybackRange(1, 24)
    hou.setFrame(1)

    for context in (object_context, output_context):
        context.layoutChildren()

    output_path.parent.mkdir(parents=True, exist_ok=True)
    hou.hipFile.save(file_name=str(output_path), save_to_recent_files=False)


def main() -> int:
    """Parse the destination and generate the demo scene."""
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    arguments = parser.parse_args()
    destination = arguments.output.resolve()
    generate(destination)
    print(f"Generated native HouIO ROP demo: {destination}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
