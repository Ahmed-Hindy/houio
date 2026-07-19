"""Inspect HOM SOP verbs needed by the HouIO VDB bridge."""

from __future__ import annotations

import hou


def describe_verb(name: str) -> None:
    """Print parameter names and menu values for a SOP verb."""
    verb = hou.sopNodeTypeCategory().nodeVerb(name)
    print(f"verb={name} available={verb is not None}")
    if verb is None:
        return
    print(f"methods={[item for item in dir(verb) if not item.startswith('_')]}")
    print(f"parms={verb.parms()}")
    node_type = hou.nodeType(hou.sopNodeTypeCategory(), name)
    if node_type is not None:
        for parameter_name in ("conversion", "vdbclass", "vdbtype", "vdbprecision"):
            template = node_type.parmTemplateGroup().find(parameter_name)
            if template is not None and hasattr(template, "menuItems"):
                print(
                    "menu",
                    template.name(),
                    tuple(template.menuItems()),
                    tuple(template.menuLabels()),
                )


def main() -> int:
    """Inspect VDB creation and conversion verbs."""
    describe_verb("vdb")
    describe_verb("convertvdb")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
