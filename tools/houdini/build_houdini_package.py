"""Build the distributable HouIO Houdini package archive."""

from __future__ import annotations

import argparse
import shutil
import tempfile
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PACKAGE_SOURCE_ROOT = PROJECT_ROOT / "houdini" / "package"
PYTHON_PACKAGE_ROOT = PROJECT_ROOT / "python" / "houio_hom"
INSTALLER_PATH = PROJECT_ROOT / "tools" / "houdini" / "install_houdini_package.ps1"


def parse_arguments() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--converter", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--version", required=True)
    return parser.parse_args()


def copy_python_package(destination: Path) -> None:
    """Copy the HOM bridge without interpreter cache files.

    Args:
        destination: Destination directory for ``houio_hom``.
    """
    shutil.copytree(
        PYTHON_PACKAGE_ROOT,
        destination,
        ignore=shutil.ignore_patterns("__pycache__", "*.pyc"),
    )


def write_package_file(destination: Path, version: str) -> None:
    """Write the root Houdini package file.

    Args:
        destination: Destination JSON file.
        version: HouIO package version.
    """
    template_path = PACKAGE_SOURCE_ROOT / "houio.json.in"
    package_text = template_path.read_text(encoding="utf-8")
    destination.write_text(
        package_text.replace("@HOUIO_VERSION@", version),
        encoding="utf-8",
    )


def build_package(converter: Path, output: Path, version: str) -> Path:
    """Build a ZIP archive installable through Houdini's Package Browser.

    Args:
        converter: Compiled ``houio_convert`` executable.
        output: Destination ZIP path.
        version: HouIO package version.

    Returns:
        The resolved archive path.

    Raises:
        FileNotFoundError: If required source files are missing.
    """
    converter = converter.resolve()
    output = output.resolve()
    if not converter.is_file():
        raise FileNotFoundError(f"Converter does not exist: {converter}")
    if not PACKAGE_SOURCE_ROOT.is_dir():
        raise FileNotFoundError(f"Package source does not exist: {PACKAGE_SOURCE_ROOT}")
    if not PYTHON_PACKAGE_ROOT.is_dir():
        raise FileNotFoundError(f"Python bridge does not exist: {PYTHON_PACKAGE_ROOT}")
    if not INSTALLER_PATH.is_file():
        raise FileNotFoundError(f"Installer does not exist: {INSTALLER_PATH}")

    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        output.unlink()

    with tempfile.TemporaryDirectory(prefix="houio_houdini_package_") as temporary_directory:
        staging_root = Path(temporary_directory)
        content_root = staging_root / "houio"
        shutil.copytree(PACKAGE_SOURCE_ROOT / "houio", content_root)
        copy_python_package(content_root / "scripts" / "python" / "houio_hom")

        binary_root = content_root / "bin"
        binary_root.mkdir(parents=True, exist_ok=True)
        shutil.copy2(converter, binary_root / converter.name)
        shutil.copy2(INSTALLER_PATH, staging_root / INSTALLER_PATH.name)
        write_package_file(staging_root / "houio.json", version)

        archive_base = output.with_suffix("")
        generated_archive = Path(
            shutil.make_archive(
                str(archive_base),
                "zip",
                root_dir=staging_root,
            )
        )
        if generated_archive != output:
            generated_archive.replace(output)

    return output


def main() -> int:
    """Build the requested package archive."""
    arguments = parse_arguments()
    archive_path = build_package(
        arguments.converter,
        arguments.output,
        arguments.version,
    )
    print(archive_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
