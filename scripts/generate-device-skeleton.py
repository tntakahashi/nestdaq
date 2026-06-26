#!/usr/bin/env python3
"""Generate a minimal NestDAQ FairMQ device skeleton."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


CLASS_NAME_PATTERN = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
TEMPLATE_FILES = {
    "Device.h.in": "{class_name}.h",
    "Device.cxx.in": "{class_name}.cxx",
    "CMakeLists.txt.in": "CMakeLists.txt",
    "README.md.in": "README.md",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate a minimal NestDAQ FairMQ device skeleton."
    )
    parser.add_argument("class_name", help="C++ device class name to generate")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="Output directory. Defaults to ./CLASS_NAME.",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Overwrite existing generated files.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print files that would be generated without writing them.",
    )
    return parser.parse_args()


def validate_class_name(class_name: str) -> None:
    if not CLASS_NAME_PATTERN.match(class_name):
        raise ValueError(
            f"invalid C++ class name '{class_name}'; expected pattern "
            f"{CLASS_NAME_PATTERN.pattern}"
        )


def find_template_dir(script_path: Path) -> Path:
    repo_or_prefix = script_path.parent.parent
    candidates = [
        repo_or_prefix / "share" / "device-skeleton",
        repo_or_prefix / "share" / "nestdaq" / "device-skeleton",
    ]
    for candidate in candidates:
        if all((candidate / template).is_file() for template in TEMPLATE_FILES):
            return candidate

    searched = "\n  ".join(str(candidate) for candidate in candidates)
    raise FileNotFoundError(f"device skeleton templates were not found under:\n  {searched}")


def render_template(template: str, substitutions: dict[str, str]) -> str:
    rendered = template
    for key, value in substitutions.items():
        rendered = rendered.replace(f"@{key}@", value)
    return rendered


def main() -> int:
    args = parse_args()
    class_name = args.class_name

    try:
        validate_class_name(class_name)
        template_dir = find_template_dir(Path(__file__).resolve())
    except (FileNotFoundError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    output_dir = args.output if args.output is not None else Path.cwd() / class_name
    output_dir = output_dir.expanduser()

    substitutions = {
        "CLASS_NAME": class_name,
        "HEADER_FILE": f"{class_name}.h",
        "SOURCE_FILE": f"{class_name}.cxx",
    }

    planned_files = [
        output_dir / output_name.format(class_name=class_name)
        for output_name in TEMPLATE_FILES.values()
    ]

    if args.dry_run:
        print(f"Template directory: {template_dir}")
        print(f"Output directory: {output_dir}")
        for path in planned_files:
            print(f"would generate: {path}")
        return 0

    existing_files = [path for path in planned_files if path.exists()]
    if existing_files and not args.force:
        print("error: refusing to overwrite existing files:", file=sys.stderr)
        for path in existing_files:
            print(f"  {path}", file=sys.stderr)
        print("rerun with --force to overwrite them", file=sys.stderr)
        return 1

    output_dir.mkdir(parents=True, exist_ok=True)
    for template_name, output_pattern in TEMPLATE_FILES.items():
        output_path = output_dir / output_pattern.format(class_name=class_name)
        template = (template_dir / template_name).read_text(encoding="utf-8")
        output_path.write_text(render_template(template, substitutions), encoding="utf-8")
        print(f"generated: {output_path}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
