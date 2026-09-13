#!/usr/bin/env python3
"""Run one command and retain local, hashed implementation/test evidence.

This explicit development tool is never used by ordinary library builds.
Records contain local absolute paths and must be sanitized before publication.
Commands are argument vectors, never shell text. Output files are exclusive:
each invocation needs a fresh directory outside the source tree.
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
import pathlib
import platform
import shutil
import subprocess
import sys
import xml.etree.ElementTree as element_tree


def sha256_file(path: pathlib.Path) -> str:
    """Return a file's SHA-256 without loading the entire file into memory."""
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _git_output(source: pathlib.Path, arguments: list[str]) -> bytes | None:
    result = subprocess.run(
        ["git", "-C", str(source), *arguments],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )
    return result.stdout if result.returncode == 0 else None


def source_identity(source: pathlib.Path) -> dict:
    """Capture Git identity and content hashes, including untracked inputs.

    An archive directory has no asserted Git identity. Its complete file list
    is hashed so the caller can tie it to a separately verified source archive.
    """
    git_root = _git_output(source, ["rev-parse", "--show-toplevel"])
    is_checkout = (
        git_root is not None
        and pathlib.Path(os.fsdecode(git_root).strip()).resolve() == source
    )
    commit = None
    tree = None
    diff_hash = None
    if is_checkout:
        commit = _git_output(source, ["rev-parse", "HEAD"]).decode().strip()
        tree = (
            _git_output(source, ["rev-parse", "HEAD^{tree}"]).decode().strip()
        )
        diff_hash = hashlib.sha256(
            _git_output(source, ["diff", "--binary", "HEAD"])
        ).hexdigest()
        listing = _git_output(
            source,
            ["ls-files", "--cached", "--others", "--exclude-standard", "-z"],
        )
        relative_paths = {
            os.fsdecode(path) for path in listing.split(b"\0") if path
        }
    else:
        relative_paths = {
            path.relative_to(source).as_posix()
            for path in source.rglob("*")
            if path.is_file()
        }
    files = {}
    for name in sorted(relative_paths):
        path = source / name
        if path.is_symlink():
            files[name] = {"symlink": os.readlink(path)}
        elif path.is_file():
            files[name] = {"sha256": sha256_file(path)}
        else:
            files[name] = {"missing": True}
    serialized = json.dumps(files, sort_keys=True, separators=(",", ":"))
    return {
        "commit": commit,
        "tree": tree,
        "dirty_diff_sha256": diff_hash,
        "content_sha256": hashlib.sha256(serialized.encode()).hexdigest(),
        "files": files,
    }


def ctest_counts(path: pathlib.Path) -> dict[str, int]:
    """Read actual CTest JUnit cases; reject absent, malformed, or empty runs."""
    root = element_tree.parse(path).getroot()
    cases = list(root.iter("testcase"))
    if not cases:
        raise ValueError("CTest evidence contains zero test cases")
    skipped = sum(case.find("skipped") is not None for case in cases)
    failed = sum(
        case.find("failure") is not None or case.find("error") is not None
        for case in cases
    )
    return {
        "selected": len(cases),
        "executed": len(cases) - skipped,
        "passed": len(cases) - skipped - failed,
        "failed": failed,
        "skipped": skipped,
    }


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--record-dir", type=pathlib.Path, required=True)
    parser.add_argument("--source-root", type=pathlib.Path, required=True)
    parser.add_argument("--cwd", type=pathlib.Path, required=True)
    parser.add_argument("--configuration", required=True)
    parser.add_argument("--id", required=True)
    parser.add_argument("--metadata", type=pathlib.Path)
    parser.add_argument(
        "--manifest", action="append", default=[], type=pathlib.Path
    )
    parser.add_argument("--covered-id", action="append", default=[])
    parser.add_argument(
        "--ctest",
        action="store_true",
        help="Append --no-tests=error and --output-junit; reject skips/zero tests.",
    )
    parser.add_argument("command", nargs=argparse.REMAINDER)
    return parser


def _prepare_command(
    arguments: argparse.Namespace, record_dir: pathlib.Path
) -> list[str]:
    command = arguments.command
    if command and command[0] == "--":
        command = command[1:]
    if not command:
        raise ValueError(
            "an executable and argument vector are required after --"
        )
    if arguments.ctest:
        if pathlib.Path(command[0]).name not in ("ctest", "ctest.exe"):
            raise ValueError("--ctest requires the ctest executable")
        if any(value.startswith("--output-junit") for value in command):
            raise ValueError("the runner owns --output-junit")
        command = [
            *command,
            "--no-tests=error",
            "--output-junit",
            str(record_dir / "ctest.xml"),
        ]
    return command


def _execute(
    command: list[str], cwd: pathlib.Path, record_dir: pathlib.Path
) -> dict:
    started = datetime.datetime.now(datetime.timezone.utc).isoformat()
    log_path = record_dir / "command.log"
    with log_path.open("xb") as log:
        result = subprocess.run(
            command, cwd=cwd, stdout=log, stderr=subprocess.STDOUT, check=False
        )
        log.flush()
        os.fsync(log.fileno())
    completed = datetime.datetime.now(datetime.timezone.utc).isoformat()
    return {
        "started": started,
        "completed": completed,
        "command_exit_code": result.returncode,
    }


def _collect_result(
    record: dict, record_dir: pathlib.Path, expect_ctest: bool
) -> dict:
    failures = []
    counts = None
    if (
        record["source_before"]["content_sha256"]
        != record["source_after"]["content_sha256"]
    ):
        failures.append("source content changed while command ran")
    artifacts = {"command.log": sha256_file(record_dir / "command.log")}
    if expect_ctest:
        junit_path = record_dir / "ctest.xml"
        try:
            counts = ctest_counts(junit_path)
            if counts["skipped"]:
                failures.append("CTest skipped one or more selected tests")
            if counts["failed"]:
                failures.append("CTest recorded one or more failed tests")
        except (OSError, ValueError, element_tree.ParseError) as error:
            failures.append(str(error))
        if junit_path.exists():
            artifacts["ctest.xml"] = sha256_file(junit_path)
    return {
        "evidence_exit_code": record["command_exit_code"]
        or int(bool(failures)),
        "evidence_failures": failures,
        "test_counts": counts,
        "artifacts": artifacts,
    }


def run(arguments: argparse.Namespace) -> int:
    """Run the requested command, returning its failure or an evidence failure.

    Source changes during execution invalidate the evidence. The original
    command exit remains separately recorded even when evidence checks fail.
    """
    source = arguments.source_root.resolve(strict=True)
    cwd = arguments.cwd.resolve(strict=True)
    record_dir = arguments.record_dir.resolve()
    if record_dir == source or source in record_dir.parents:
        raise ValueError("evidence directory must be outside the source tree")
    command = _prepare_command(arguments, record_dir)
    metadata = {}
    if arguments.metadata:
        metadata = json.loads(arguments.metadata.read_text(encoding="utf-8"))
        if not isinstance(metadata, dict):
            raise ValueError("metadata must be a JSON object")
    manifests = {}
    for manifest in arguments.manifest:
        path = manifest if manifest.is_absolute() else source / manifest
        manifests[str(manifest)] = sha256_file(path)
    record = {
        "schema_version": 1,
        "id": arguments.id,
        "source_root": str(source),
        "source_before": source_identity(source),
        "configuration": arguments.configuration,
        "metadata": metadata,
        "manifest_sha256": manifests,
        "covered_ids": arguments.covered_id,
        "command": command,
        "cwd": str(cwd),
        "executable": shutil.which(command[0]),
        "platform": {
            "system": platform.system(),
            "release": platform.release(),
            "machine": platform.machine(),
            "python": platform.python_version(),
        },
    }
    record_dir.mkdir(parents=True, exist_ok=False)
    record.update(_execute(command, cwd, record_dir))
    record["source_after"] = source_identity(source)
    record.update(_collect_result(record, record_dir, arguments.ctest))
    with (record_dir / "record.json").open("x", encoding="utf-8") as stream:
        json.dump(record, stream, indent=2, sort_keys=True)
        stream.write("\n")
    print(
        json.dumps(
            {
                "record": str(record_dir / "record.json"),
                "exit_code": record["evidence_exit_code"],
                "test_counts": record["test_counts"],
                "failures": record["evidence_failures"],
            },
            sort_keys=True,
        )
    )
    exit_code = record["evidence_exit_code"]
    return exit_code if exit_code >= 0 else 128 - exit_code


def main() -> int:
    """Parse the CLI and report operational failures without hiding their exit."""
    arguments = _parser().parse_args()
    try:
        return run(arguments)
    except (OSError, ValueError, element_tree.ParseError) as error:
        print(f"evidence error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
