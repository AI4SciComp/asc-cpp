#!/usr/bin/env python3
"""Reject missing tests, failures, errors, and unexpected CTest skips."""

from __future__ import annotations

import argparse
import xml.etree.ElementTree as ET
from pathlib import Path


def integer(root: ET.Element, name: str) -> int:
    return int(root.get(name, "0"))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("junit", type=Path)
    args = parser.parse_args()
    if not args.junit.is_file():
        raise SystemExit(f"CTest JUnit result is missing: {args.junit}")

    root = ET.parse(args.junit).getroot()
    suites = [root] if root.tag == "testsuite" else list(root.findall("testsuite"))
    tests = sum(integer(suite, "tests") for suite in suites)
    failures = sum(integer(suite, "failures") for suite in suites)
    errors = sum(integer(suite, "errors") for suite in suites)
    skipped_nodes = root.findall(".//skipped")
    skipped = sum(integer(suite, "skipped") for suite in suites)
    skipped = max(skipped, len(skipped_nodes))

    if tests == 0:
        raise SystemExit("CTest reported zero tests")
    if failures or errors or skipped:
        names = [
            case.get("name", "<unnamed>")
            for case in root.findall(".//testcase")
            if case.find("failure") is not None
            or case.find("error") is not None
            or case.find("skipped") is not None
        ]
        raise SystemExit(
            f"CTest result is not clean: {tests} tests, {failures} failures, "
            f"{errors} errors, {skipped} skips; affected: {', '.join(names)}"
        )
    print(f"CTest result is clean: {tests} tests, zero failures/errors/skips")


if __name__ == "__main__":
    main()
