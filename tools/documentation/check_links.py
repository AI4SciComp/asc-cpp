#!/usr/bin/env python3
"""Check repository-local Markdown links without contacting the network."""

from __future__ import annotations

import argparse
import re
import subprocess
import urllib.parse
from pathlib import Path


LINK = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")


def markdown_files(root: Path) -> list[str]:
    try:
        result = subprocess.run(
            [
                "git", "-C", str(root), "ls-files", "--cached", "--others",
                "--exclude-standard", "--", "*.md",
            ],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
        )
    except FileNotFoundError:
        result = None
    if result is not None and result.returncode == 0:
        candidates = result.stdout.splitlines()
    else:
        candidates = sorted(
            path.relative_to(root).as_posix()
            for path in root.rglob("*.md")
            if ".git" not in path.relative_to(root).parts
        )
    return [relative for relative in candidates if (root / relative).is_file()]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    args = parser.parse_args()
    root = args.root.resolve()
    tracked = markdown_files(root)
    failures: list[str] = []
    for relative in tracked:
        source = root / relative
        for line_number, line in enumerate(
            source.read_text(encoding="utf-8").splitlines(), start=1
        ):
            for match in LINK.finditer(line):
                raw = match.group(1).strip()
                if raw.startswith("<") and ">" in raw:
                    raw = raw[1 : raw.index(">")]
                elif " \"" in raw:
                    raw = raw.split(" \"", 1)[0]
                target = urllib.parse.unquote(raw.split("#", 1)[0])
                if not target or urllib.parse.urlparse(target).scheme:
                    continue
                resolved = (source.parent / target).resolve()
                try:
                    resolved.relative_to(root)
                except ValueError:
                    failures.append(
                        f"{relative}:{line_number}: link escapes repository: {raw}"
                    )
                    continue
                if not resolved.exists():
                    failures.append(
                        f"{relative}:{line_number}: missing link target: {raw}"
                    )
    if failures:
        raise SystemExit("\n".join(failures))
    print(f"checked {len(tracked)} Markdown files")


if __name__ == "__main__":
    main()
