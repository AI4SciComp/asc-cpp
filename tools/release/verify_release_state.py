#!/usr/bin/env python3
"""Verify that all ASCCpp release-version sources agree."""

from __future__ import annotations

import argparse
import re
import subprocess
from pathlib import Path


VERSION = "0.9.0"
DATE = "2026-08-03"


def require(path: Path, pattern: str) -> None:
    contents = path.read_text(encoding="utf-8")
    if not re.search(pattern, contents, re.MULTILINE):
        raise SystemExit(f"{path} does not match required pattern: {pattern}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--tag", default=f"v{VERSION}")
    parser.add_argument("--require-head-tag", action="store_true")
    args = parser.parse_args()
    source = args.source.resolve()

    if args.tag != f"v{VERSION}":
        raise SystemExit(f"tag must be exactly v{VERSION}, found {args.tag}")
    require(source / "CMakeLists.txt", rf"project\(\s*ASCCpp\s+VERSION {re.escape(VERSION)}\b")
    require(source / "CHANGELOG.md", rf"^## \[{re.escape(VERSION)}\] - {DATE}$")
    require(source / "CITATION.cff", rf'^version: ["\']?{re.escape(VERSION)}["\']?$')
    require(source / "CITATION.cff", rf'^date-released: ["\']?{DATE}["\']?$')
    require(source / "docs" / "Doxyfile.in", r'^PROJECT_NUMBER\s*=\s*"@PROJECT_VERSION@"$')
    require(source / "release" / "release-notes-v0.9.0.md", rf"^# ASCCpp {re.escape(VERSION)} release notes$")
    require(source / "release" / "known-limitations-v0.9.0.md", rf"^# ASCCpp {re.escape(VERSION)} known limitations$")

    development = source / "docs" / "development"
    if development.exists() and any(
        path.is_file() or path.is_symlink() for path in development.rglob("*")
    ):
        raise SystemExit("docs/development is not empty")

    if args.require_head_tag:
        head = subprocess.check_output(
            ["git", "-C", str(source), "rev-parse", "HEAD"], text=True
        ).strip()
        tagged = subprocess.check_output(
            ["git", "-C", str(source), "rev-list", "-n", "1", args.tag],
            text=True,
        ).strip()
        if head != tagged:
            raise SystemExit(f"{args.tag} resolves to {tagged}, not HEAD {head}")

    print(f"release state is consistent for ASCCpp {VERSION} ({DATE})")


if __name__ == "__main__":
    main()
