#!/usr/bin/env python3
"""Inspect ASCCpp source and installed tar archives without extracting them."""

from __future__ import annotations

import argparse
import tarfile
from pathlib import Path, PurePosixPath


SOURCE_REQUIRED = {
    "LICENSE",
    "THIRD_PARTY_NOTICES",
    "CMakeLists.txt",
    "CITATION.cff",
    "data/random/LICENSE.joe-kuo",
    "data/random/new-joe-kuo-6.21201",
    "include/asc/core.h",
    "cmake/AcquireASCCMake.cmake",
}
INSTALL_REQUIRED_SUFFIXES = {
    "include/asc/core.h",
    "lib/cmake/ASCCpp/ASCCppConfig.cmake",
    "lib/cmake/ASCCpp/ASCCppConfigVersion.cmake",
    "share/doc/ASCCpp/LICENSE",
    "share/doc/ASCCpp/THIRD_PARTY_NOTICES",
    "share/doc/ASCCpp/random/LICENSE.joe-kuo",
    "share/doc/ASCCpp/random/new-joe-kuo-6.21201",
}
INSTALL_REQUIRED_LIBRARIES = {
    "asc_core",
    "asc_dense",
    "asc_random",
    "asc_sparse",
    "asc_utilities",
}


def resolve_link(path: PurePosixPath, linkname: str) -> PurePosixPath:
    link = PurePosixPath(linkname)
    if link.is_absolute():
        raise ValueError("absolute target")
    parts = list(path.parent.parts)
    for part in link.parts:
        if part in ("", "."):
            continue
        if part == "..":
            if len(parts) <= 1:
                raise ValueError("target escapes archive root")
            parts.pop()
        else:
            parts.append(part)
    return PurePosixPath(*parts)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    parser.add_argument("--kind", choices=("source", "install"), required=True)
    parser.add_argument(
        "--forbid-bytes",
        action="append",
        default=[],
        help="reject a regular member containing this exact UTF-8 string",
    )
    args = parser.parse_args()

    with tarfile.open(args.archive, "r:gz") as archive:
        members = archive.getmembers()
        if not members:
            raise SystemExit(f"archive is empty: {args.archive}")
        member_names = {PurePosixPath(member.name) for member in members}
        names: set[str] = set()
        seen: set[PurePosixPath] = set()
        roots: set[str] = set()
        if any(not value for value in args.forbid_bytes):
            raise SystemExit("--forbid-bytes markers must not be empty")
        forbidden = [value.encode("utf-8") for value in args.forbid_bytes]
        for member in members:
            path = PurePosixPath(member.name)
            if not path.parts or path.is_absolute() or ".." in path.parts:
                raise SystemExit(f"unsafe archive member: {member.name}")
            if path in seen:
                raise SystemExit(f"duplicate archive member: {member.name}")
            seen.add(path)
            roots.add(path.parts[0])
            if member.islnk():
                raise SystemExit(f"hard links are not permitted: {member.name}")
            if not (member.isdir() or member.isfile() or member.issym()):
                raise SystemExit(f"special archive member is not permitted: {member.name}")
            if member.issym():
                if args.kind == "source":
                    raise SystemExit(
                        f"links are not permitted in source archives: {member.name}"
                    )
                try:
                    target = resolve_link(path, member.linkname)
                except ValueError as error:
                    raise SystemExit(
                        f"unsafe link {member.name} -> {member.linkname}: {error}"
                    ) from error
                if target not in member_names:
                    raise SystemExit(
                        f"link target is absent: {member.name} -> {member.linkname}"
                    )
            if member.isfile() and forbidden:
                extracted = archive.extractfile(member)
                payload = b"" if extracted is None else extracted.read()
                for marker, marker_bytes in zip(args.forbid_bytes, forbidden):
                    if marker_bytes in payload:
                        raise SystemExit(
                            f"forbidden path marker {marker!r} in {member.name}"
                        )
            if len(path.parts) > 1:
                names.add(PurePosixPath(*path.parts[1:]).as_posix())

    if len(roots) != 1:
        raise SystemExit(
            f"archive must contain exactly one top-level root, found: "
            + ", ".join(sorted(roots))
        )

    required = SOURCE_REQUIRED if args.kind == "source" else INSTALL_REQUIRED_SUFFIXES
    missing = sorted(required - names)
    if missing:
        raise SystemExit(f"{args.archive} omits: {', '.join(missing)}")
    if args.kind == "install":
        missing_libraries = sorted(
            library
            for library in INSTALL_REQUIRED_LIBRARIES
            if not any(
                name == f"lib/lib{library}.a"
                or name == f"lib/lib{library}.so.0.9.0"
                for name in names
            )
        )
        if missing_libraries:
            raise SystemExit(
                f"{args.archive} omits installed libraries: "
                + ", ".join(missing_libraries)
            )
    print(f"{args.archive}: {len(members)} safe members; required {args.kind} content present")


if __name__ == "__main__":
    main()
