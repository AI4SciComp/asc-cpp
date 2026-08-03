#!/usr/bin/env python3
"""Enforce ASCCpp Doxygen coverage and reproducibility contracts."""

from __future__ import annotations

import argparse
import re
import xml.etree.ElementTree as ET
from pathlib import Path


INTERNAL = re.compile(r"(^|::)internal[^:]*($|::)|_internal($|::)")


def text(element: ET.Element | None) -> str:
    return "" if element is None else "".join(element.itertext()).strip()


def documented(element: ET.Element) -> bool:
    return bool(text(element.find("briefdescription")) or
                text(element.find("detaileddescription")))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--xml", type=Path, required=True)
    parser.add_argument("--html", type=Path, required=True)
    parser.add_argument("--tagfile", type=Path, required=True)
    parser.add_argument("--warnings", type=Path, required=True)
    args = parser.parse_args()

    required = [args.xml / "index.xml", args.html / "index.html", args.tagfile,
                args.warnings]
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise SystemExit("missing Doxygen outputs: " + ", ".join(missing))
    warnings = args.warnings.read_text(encoding="utf-8").strip()
    if warnings:
        raise SystemExit("Doxygen warnings were recorded:\n" + warnings)

    expected_headers = {
        path.relative_to(args.source).as_posix()
        for path in (args.source / "include" / "asc").rglob("*.h")
    }
    observed_headers: set[str] = set()
    undocumented: list[str] = []
    public_members = 0
    xml_files = list(args.xml.glob("*.xml"))
    for xml_file in xml_files:
        if xml_file.name == "index.xml":
            continue
        root = ET.parse(xml_file).getroot()
        compound = root.find("compounddef")
        if compound is None:
            continue
        compound_name = text(compound.find("compoundname"))
        location = compound.find("location")
        location_file = "" if location is None else location.get("file", "")
        kind = compound.get("kind", "")
        if kind == "file" and location_file.startswith("include/asc/"):
            observed_headers.add(location_file)
        if (location_file.startswith("include/asc/") and
                compound.get("prot", "public") == "public" and
                kind in {"class", "struct", "union", "concept"} and
                not INTERNAL.search(compound_name)):
            if not documented(compound):
                undocumented.append(compound_name)
        for member in compound.findall(".//memberdef"):
            if member.get("prot") != "public":
                continue
            member_location = member.find("location")
            member_file = ("" if member_location is None
                           else member_location.get("file", ""))
            if not member_file.startswith("include/asc/"):
                continue
            qualified = text(member.find("qualifiedname")) or text(member.find("name"))
            if INTERNAL.search(qualified):
                continue
            public_members += 1
            if not documented(member):
                undocumented.append(qualified)

    missing_headers = sorted(expected_headers - observed_headers)
    if missing_headers:
        raise SystemExit("headers absent from Doxygen XML:\n" + "\n".join(missing_headers))
    if undocumented:
        raise SystemExit(
            "undocumented public compounds/members:\n" +
            "\n".join(sorted(set(undocumented)))
        )

    forbidden = [str(args.source.resolve()), str(args.build.resolve()), "/home/"]
    leaked: list[str] = []
    generated = list(args.html.rglob("*")) + list(args.xml.rglob("*")) + [args.tagfile]
    for path in generated:
        if not path.is_file() or path.suffix.lower() in {".png", ".jpg", ".gif"}:
            continue
        contents = path.read_text(encoding="utf-8", errors="ignore")
        for marker in forbidden:
            if marker and marker in contents:
                leaked.append(f"{path}: {marker}")
    if leaked:
        raise SystemExit("machine-local path leakage:\n" + "\n".join(leaked))
    print(
        f"Doxygen coverage: {len(observed_headers)}/{len(expected_headers)} headers, "
        f"{public_members} documented public members, zero warnings"
    )


if __name__ == "__main__":
    main()
