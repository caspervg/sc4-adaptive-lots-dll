"""Rename the template and generate literal project-specific SC4 IDs."""

from __future__ import annotations

import argparse
import fnmatch
import re
import shutil
from pathlib import Path


OLD_NAME = "SC4TemplateDll"
PROJECT_NAME_PARTS = {
    "3d": "3D",
    "dbpf": "DBPF",
    "dll": "Dll",
    "fps": "FPS",
    "imgui": "ImGui",
    "nam": "NAM",
    "sc4": "SC4",
    "ui": "UI",
}
PATTERNS = (
    "*.txt", "*.md", "*.json", "*.yml", "*.yaml", "*.cmake",
    "CMakeLists.txt", "*.cpp", "*.hpp", "*.h", "*.def", "*.ini", "*.rc.in",
)


def kebab_case(value: str) -> str:
    return re.sub(r"([a-z0-9])([A-Z])", r"\1-\2", value).lower()


def cpp_project_name(repository_name: str) -> str:
    parts = re.findall(r"[A-Za-z0-9]+", repository_name)
    if not parts:
        raise ValueError("repository name must contain a letter or digit")

    name = "".join(
        PROJECT_NAME_PARTS.get(part.casefold(), part[:1].upper() + part[1:])
        for part in parts
    )
    if name.casefold().startswith("sc4"):
        suffix = name[3:]
        name = "SC4" + suffix[:1].upper() + suffix[1:]
    return name if name[:1].isalpha() else f"SC4{name}"


def default_ui(repository_name: str) -> str:
    parts = {part.casefold() for part in re.findall(r"[A-Za-z0-9]+", repository_name)}
    has_ui_hint = "imgui" in repository_name.casefold() or "UI" in repository_name or "ui" in parts
    return "imgui" if has_ui_hint else "none"


def project_id(value: str) -> int:
    hash_value = 2166136261
    for character in value.encode():
        hash_value = ((hash_value ^ character) * 16777619) & 0xFFFFFFFF
    return hash_value


def format_id(value: int) -> str:
    return f"0x{value:08X}u"


def matching_files(paths: list[Path]) -> list[Path]:
    files: set[Path] = set()
    for path in paths:
        if path.is_file():
            files.add(path)
        elif path.is_dir():
            files.update(
                candidate for candidate in path.rglob("*")
                if candidate.is_file()
                and any(fnmatch.fnmatch(candidate.name, pattern) for pattern in PATTERNS)
            )
    return sorted(files)


def write_text(path: Path, content: str) -> None:
    with path.open("w", encoding="utf-8", newline="") as stream:
        stream.write(content)


def materialize_variant(root: Path, variant: str) -> None:
    source = root / "templates" / variant
    destination = root / "src" / "dll"
    for name in ("SC4TemplateDllDirector.cpp", "SC4TemplateDllDirector.hpp"):
        shutil.copy2(source / name, destination / name)

    panels = destination / "panels"
    if panels.exists():
        shutil.rmtree(panels)
    if variant == "imgui":
        shutil.copytree(source / "panels", panels)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "project_name",
        help="C++ project name, for example SC4SeasonJumper",
    )
    parser.add_argument(
        "--ui",
        choices=("imgui", "none"),
        help="starter variant; repository names containing a ui or imgui part default to imgui",
    )
    parser.add_argument(
        "--repository-name",
        action="store_true",
        help="convert the positional repository name to the usual C++ project-name style",
    )
    args = parser.parse_args()

    source_name = args.project_name
    project_name = cpp_project_name(source_name) if args.repository_name else source_name
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", project_name):
        parser.error("project_name must start with a letter and contain only letters, digits, or underscores")

    root = Path(__file__).resolve().parent.parent
    variant = args.ui or (default_ui(source_name) if args.repository_name else "none")
    materialize_variant(root, variant)
    old_names = [] if project_name == OLD_NAME else [OLD_NAME]
    old_slugs = [kebab_case(name) for name in old_names]
    new_slug = kebab_case(project_name)

    for path in matching_files([
        root / ".github", root / "cmake", root / "dist", root / "src",
        root / "tools", root / "CMakeLists.txt", root / "CMakePresets.json",
        root / "README.md", root / "vcpkg.json",
    ]):
        if path == Path(__file__):
            continue
        content = path.read_text(encoding="utf-8")
        updated = content
        for old_name in old_names:
            updated = updated.replace(old_name, project_name)
        for old_slug in old_slugs:
            updated = updated.replace(old_slug, new_slug)
        if updated != content:
            write_text(path, updated)

    for directory in (root / "dist", root / "src" / "dll"):
        if not directory.is_dir():
            continue
        for path in list(directory.iterdir()):
            new_name = path.name
            for old_name in old_names:
                new_name = new_name.replace(old_name, project_name)
            if new_name != path.name:
                path.rename(path.with_name(new_name))

    director_file = next((root / "src" / "dll").glob("*Director.cpp"), None)
    if director_file:
        content = director_file.read_text(encoding="utf-8")
        director_value = project_id(source_name)
        content = content.replace("kDirectorId = 0u", f"kDirectorId = {format_id(director_value)}")
        content = content.replace("kPanelId = 0u", f"kPanelId = {format_id(director_value ^ 0xCA510001)}")
        write_text(director_file, content)

    print(f"Initialized {project_name} with the {variant} variant")


if __name__ == "__main__":
    main()
