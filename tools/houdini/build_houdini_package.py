"""Build the private-evaluation HouIO Houdini package archive."""

from __future__ import annotations

import argparse
import shutil
import tempfile
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PACKAGE_SOURCE_ROOT = PROJECT_ROOT / "houdini" / "package"
PYTHON_PACKAGE_ROOT = PROJECT_ROOT / "python" / "houio_hom"
INSTALLER_PATH = PROJECT_ROOT / "tools" / "houdini" / "install_houdini_package.ps1"
BOOTSTRAP_PATH = PROJECT_ROOT / "tools" / "houdini" / "bootstrap_houdini_package.ps1"
LICENSE_STATUS_PATH = PROJECT_ROOT / "LICENSE_STATUS.md"
THIRD_PARTY_NOTICES_PATH = PROJECT_ROOT / "THIRD_PARTY_NOTICES.md"
PROVENANCE_PATH = PROJECT_ROOT / "docs" / "provenance.md"


def parse_arguments() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--cli", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--runtime-root", type=Path)
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


def build_package(
    cli: Path,
    output: Path,
    version: str,
    runtime_root: Path | None = None,
) -> Path:
    """Build a private-evaluation ZIP installable through Houdini's Package Browser.

    Args:
        cli: Compiled primary ``houio`` executable.
        output: Destination ZIP path.
        version: HouIO package version.
        runtime_root: Optional bundled dependency prefix. Its runtime ``bin``,
            ``lib``, ``plugin``, and ``share`` trees are copied into the HouIO
            package with the same relative layout.

    Returns:
        The resolved archive path.

    Raises:
        FileNotFoundError: If required source files are missing.
    """
    cli = cli.resolve()
    output = output.resolve()
    runtime_root = runtime_root.resolve() if runtime_root is not None else None
    if not cli.is_file():
        raise FileNotFoundError(f"HouIO CLI does not exist: {cli}")
    if not PACKAGE_SOURCE_ROOT.is_dir():
        raise FileNotFoundError(f"Package source does not exist: {PACKAGE_SOURCE_ROOT}")
    if not PYTHON_PACKAGE_ROOT.is_dir():
        raise FileNotFoundError(f"Python bridge does not exist: {PYTHON_PACKAGE_ROOT}")
    if not INSTALLER_PATH.is_file():
        raise FileNotFoundError(f"Installer does not exist: {INSTALLER_PATH}")
    if not BOOTSTRAP_PATH.is_file():
        raise FileNotFoundError(f"Bootstrap script does not exist: {BOOTSTRAP_PATH}")
    for legal_document in (
        LICENSE_STATUS_PATH,
        THIRD_PARTY_NOTICES_PATH,
        PROVENANCE_PATH,
    ):
        if not legal_document.is_file():
            raise FileNotFoundError(
                f"Required legal-status document does not exist: {legal_document}"
            )
    if runtime_root is not None and not runtime_root.is_dir():
        raise FileNotFoundError(f"Runtime dependency prefix does not exist: {runtime_root}")

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
        shutil.copy2(cli, binary_root / cli.name)
        if runtime_root is not None:
            for directory_name in ("bin", "lib", "plugin", "share"):
                source_directory = runtime_root / directory_name
                if not source_directory.is_dir():
                    continue
                destination_directory = content_root / directory_name
                shutil.copytree(
                    source_directory,
                    destination_directory,
                    dirs_exist_ok=True,
                )
        legal_root = content_root / "legal"
        (legal_root / "docs").mkdir(parents=True, exist_ok=True)
        shutil.copy2(LICENSE_STATUS_PATH, legal_root / LICENSE_STATUS_PATH.name)
        shutil.copy2(
            THIRD_PARTY_NOTICES_PATH,
            legal_root / THIRD_PARTY_NOTICES_PATH.name,
        )
        shutil.copy2(PROVENANCE_PATH, legal_root / "docs" / PROVENANCE_PATH.name)

        shutil.copy2(INSTALLER_PATH, staging_root / INSTALLER_PATH.name)
        shutil.copy2(BOOTSTRAP_PATH, staging_root / BOOTSTRAP_PATH.name)
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
        arguments.cli,
        arguments.output,
        arguments.version,
        arguments.runtime_root,
    )
    print(archive_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
