"""Run the HouIO package test in a graphical Houdini session."""

from __future__ import annotations

import importlib.util
import json
import os
import traceback
from pathlib import Path
from types import ModuleType

import hou


def load_test_module(script_path: Path) -> ModuleType:
    """Load the package test module from a source path.

    Args:
        script_path: Path to ``test_houdini_package.py``.

    Returns:
        The imported test module.
    """
    specification = importlib.util.spec_from_file_location(
        "houio_houdini_package_test",
        script_path,
    )
    if specification is None or specification.loader is None:
        raise RuntimeError(f"Could not load test module: {script_path}")
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module


def main() -> None:
    """Run validation, write a JSON report, and close Houdini."""
    report_path = Path(os.environ["HOUIO_PACKAGE_GUI_REPORT"]).resolve()
    test_script = Path(os.environ["HOUIO_PACKAGE_TEST_SCRIPT"]).resolve()
    report: dict[str, object] = {
        "status": "FAIL",
        "houdini_version": hou.applicationVersionString(),
        "license_category": str(hou.licenseCategory()),
        "is_apprentice": bool(hou.isApprentice()),
    }
    exit_code = 1

    try:
        test_module = load_test_module(test_script)
        result = int(test_module.main())
        if result != 0:
            raise RuntimeError(f"Package test returned {result}")
        report["status"] = "PASS"
        exit_code = 0
    except Exception as exception:
        report["error"] = str(exception)
        report["traceback"] = traceback.format_exc()
    finally:
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")

    hou.exit(exit_code=exit_code, suppress_save_prompt=True)


main()
