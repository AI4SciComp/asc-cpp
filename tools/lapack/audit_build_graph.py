#!/usr/bin/env python3
# Copyright 2026 AI4SciComp contributors
# SPDX-License-Identifier: Apache-2.0
"""Audit pinned source-list conditions and attested archive symbols offline.

This restricted reader never runs upstream CMake. It follows only the reviewed
literal/list prefix before target creation; unsupported syntax fails closed.
It does not establish linkability, ABI correctness, numerical correctness or
ASC capability. Output must be outside the ASC source tree.
"""

from __future__ import annotations

import argparse
import collections
import dataclasses
import json
import pathlib
import posixpath
import re
import shlex
import subprocess
import sys

import attest_provider
import generate_inventory


@dataclasses.dataclass(frozen=True)
class Source:
    """A source occurrence, its conjunction of options and expansion history."""

    path: str
    conditions: tuple[str, ...]
    history: tuple[str, ...]


def commands(text: str) -> list[tuple[str, list[str], int]]:
    """Read balanced CMake command arguments without interpreting their content.

    Quoted arguments and line comments are recognized. Bracket strings, bracket
    comments and escapes are deliberately rejected: the audited prefixes do not
    need them. The reader therefore cannot silently misinterpret new syntax.
    """
    if "[[" in text or "[=[" in text or "\\" in text:
        raise ValueError("Unsupported CMake bracket or escape syntax")
    result = []
    position = 0
    while position < len(text):
        match = re.match(r"\s+|#[^\n]*", text[position:])
        if match:
            position += match.end()
            continue
        match = re.match(r"([A-Za-z_]\w*)\s*\(", text[position:])
        if not match:
            raise ValueError(f"Malformed CMake near offset {position}")
        line = text.count("\n", 0, position) + 1
        name = match.group(1).lower()
        position += match.end()
        start = position
        depth = 1
        quoted = False
        while position < len(text) and depth:
            char = text[position]
            if char == '"':
                quoted = not quoted
            elif not quoted and char == "#":
                end = text.find("\n", position)
                position = len(text) if end < 0 else end
                continue
            elif not quoted and char == "(":
                depth += 1
            elif not quoted and char == ")":
                depth -= 1
            position += 1
        if depth or quoted:
            raise ValueError("Unterminated CMake command")
        lexer = shlex.shlex(text[start:position - 1], posix=True)
        lexer.whitespace_split = True
        arguments = list(lexer)
        result.append((name, arguments, line))
    return result


def list_graph(text: str, path: str,
               children: dict[str, dict[str, list[Source]]] | None = None
               ) -> dict[str, list[Source]]:
    """Expand reviewed source lists, retaining duplicate provenance and guards.

    Supported commands are set, list(APPEND/REMOVE_DUPLICATES), simple positive
    if/endif, and the pinned append_subdir_files macro at its call site. The
    caller passes only the source-list prefix; macro bodies are not interpreted.
    Timing-source substitutions remain explicit unresolved tokens.
    """
    # Keep the deliberately small accepted-command table in one reviewable loop.
    # pylint: disable=too-many-branches,too-many-statements
    variables = {}
    conditions = []
    children = children or {}
    base = posixpath.dirname(path)

    def expand(tokens: list[str], destination: str, line: int) -> list[Source]:
        result = []
        step = f"{path}:{line}:{destination}"
        for token in tokens:
            match = re.fullmatch(r"\$\{(\w+)\}", token)
            if match:
                variable = match.group(1)
                if variable in ("SECOND_SRC", "DSECOND_SRC"):
                    values = [Source(token, (), ())]
                elif variable in variables:
                    values = variables[variable]
                else:
                    raise ValueError(f"Undefined list {variable} at {step}")
                result.extend(Source(value.path,
                                     tuple(sorted(set(value.conditions +
                                                      tuple(conditions)))),
                                     value.history + (step,))
                              for value in values)
            else:
                if "$" in token or ";" in token:
                    raise ValueError(f"Unsupported list token {token} at {step}")
                result.append(Source(posixpath.normpath(f"{base}/{token}"),
                                     tuple(conditions), (step,)))
        return result

    for name, arguments, line in commands(text):
        if name == "if":
            if len(arguments) != 1 or not re.fullmatch(r"[A-Z][A-Z0-9_]*",
                                                      arguments[0]):
                raise ValueError("Only simple positive option guards supported")
            conditions.append(arguments[0])
        elif name == "endif":
            if arguments or not conditions:
                raise ValueError("Unbalanced or nonempty endif")
            conditions.pop()
        elif name == "set":
            if not arguments or conditions:
                raise ValueError("Conditional replacement set is not supported")
            variables[arguments[0]] = expand(arguments[1:], arguments[0], line)
        elif name == "list" and arguments[:1] == ["APPEND"]:
            if len(arguments) < 2:
                raise ValueError("Missing list destination")
            destination = arguments[1]
            variables.setdefault(destination, []).extend(
                expand(arguments[2:], destination, line))
        elif name == "list" and arguments[:1] == ["REMOVE_DUPLICATES"]:
            if len(arguments) != 2 or arguments[1] not in variables:
                raise ValueError("Invalid duplicate removal")
            # Preserve occurrences for review; source selection is set-valued.
        elif name == "append_subdir_files" and len(arguments) == 2:
            variable, child = arguments
            if variable == "LAPACKE_INCLUDE" and child == "include":
                continue  # Header installation does not select object sources.
            if child not in children or variable not in children[child]:
                raise ValueError("Unrecognized child-list transfer")
            step = f"{path}:{line}:{variable}"
            variables.setdefault(variable, []).extend(
                Source(value.path,
                       tuple(sorted(set(value.conditions + tuple(conditions)))),
                       value.history + (step,))
                for value in children[child][variable])
        else:
            raise ValueError(f"Unsupported source-list command {name}:{line}")
    if conditions:
        raise ValueError("Unterminated source-list guard")
    return variables


def normalized_sources(values: list[Source]) -> dict:
    """Merge source occurrences into exact OR-of-AND guards and provenance."""
    grouped = collections.defaultdict(list)
    for value in values:
        grouped[value.path].append(value)
    result = {}
    for path, occurrences in sorted(grouped.items()):
        terms = sorted(set(value.conditions for value in occurrences))
        result[path] = {
            "conditions_dnf": [list(term) for term in terms],
            "expansion_paths": sorted(set(value.history for value in occurrences)),
            "occurrences_before_deduplication": len(occurrences),
        }
    return result


def selected(record: dict, options: dict[str, str]) -> bool:
    """Evaluate positive build-option conjunctions, rejecting missing options."""
    for term in record["conditions_dnf"]:
        for option in term:
            if option not in options or options[option] not in ("ON", "OFF"):
                raise ValueError(f"Missing/nonboolean option {option}")
    return any(all(options[option] == "ON" for option in term)
               for term in record["conditions_dnf"])


def parse_symbols(text: str, archive: str) -> dict[str, list[dict]]:
    """Parse GNU nm POSIX defined-global output with archive member identities."""
    result = collections.defaultdict(list)
    member = None
    for line in text.splitlines():
        if not line:
            continue
        match = re.fullmatch(r".*\[([^\]]+)\]:", line)
        if match:
            member = match.group(1)
            continue
        fields = line.split()
        if member is None or len(fields) not in (3, 4) or len(fields[1]) != 1:
            raise ValueError(f"Unrecognized nm record {line}")
        if fields[1].upper() == "U":
            raise ValueError("Defined-only symbol report contains undefined entry")
        result[fields[0]].append({"archive": archive, "member": member,
                                  "type": fields[1]})
    return dict(result)


def source_graph(source_root: pathlib.Path, inventory: dict) -> dict:
    """Check frozen source bytes and collect the three reviewed object graphs."""
    if generate_inventory.verify_source(
            source_root, inventory["specification"]["commit"]
    ) != inventory["specification"]:
        raise ValueError("Source specification differs from inventory")
    for entry in inventory["source_inputs"]:
        if generate_inventory.sha256((source_root / entry["path"]).read_bytes()
                                     ) != entry["sha256"]:
            raise ValueError(f"Source differs from frozen inventory: {entry['path']}")
    result = {}
    for library, path in (("lapack", "SRC/CMakeLists.txt"),
                          ("blas", "BLAS/SRC/CMakeLists.txt")):
        text = (source_root / path).read_text(encoding="utf-8")
        prefix = text[:text.index("add_library(")]
        variables = list_graph(prefix, path)
        result[library] = normalized_sources(
            variables["SOURCES"] + variables.get("ALLMOD", []))
    children = {}
    for child in ("src", "utils"):
        path = f"LAPACKE/{child}/CMakeLists.txt"
        children[child] = list_graph((source_root / path).read_text(
            encoding="utf-8"), path)
    path = "LAPACKE/CMakeLists.txt"
    text = (source_root / path).read_text(encoding="utf-8")
    start = text.index('append_subdir_files(LAPACKE_INCLUDE "include")')
    # Keep physical line numbers while skipping the separately reviewed macro.
    prefix = "\n" * text.count("\n", 0, start) + text[start:text.index("add_library(")]
    result["lapacke"] = normalized_sources(
        list_graph(prefix, path, children)["SOURCES"])
    return result


def provider_symbols(label: str, attestation_path: pathlib.Path,
                     prefix: pathlib.Path, args: argparse.Namespace) -> dict:
    """Verify an existing attestation, then read exact archives with GNU nm."""
    record = json.loads(attestation_path.read_text(encoding="utf-8"))
    record = attest_provider.verify_attestation(argparse.Namespace(
        attestation=attestation_path, prefix=prefix, inventory=args.inventory,
        provider_lock=args.provider_lock, integer_bits=record["payload"]["integer_bits"]))
    payload = record["payload"]
    suffix = "64" if payload["integer_bits"] == 64 else ""
    symbols = collections.defaultdict(list)
    archives = []
    for library in ("blas", "lapack", "lapacke"):
        path = f"lib/lib{library}{suffix}.a"
        raw = subprocess.run([args.nm, "-g", "--defined-only", "--format=posix",
                              str(prefix / path)], check=True,
                             capture_output=True, text=True).stdout
        for name, definitions in parse_symbols(raw, path).items():
            symbols[name].extend(definitions)
        archives.append({"path": path,
                         "sha256": generate_inventory.sha256((prefix / path).read_bytes()),
                         "nm_stdout_sha256": generate_inventory.sha256(raw.encode()),
                         "nm_stdout": raw.replace(str(prefix) + "/", "")})
    option_names = {"BUILD_SINGLE", "BUILD_DOUBLE", "BUILD_COMPLEX", "BUILD_COMPLEX16",
                    "BUILD_DEPRECATED", "BUILD_INDEX64", "BUILD_INDEX64_EXT_API",
                    "LAPACKE", "LAPACKE_WITH_TMG", "USE_XBLAS", "USE_OPTIMIZED_BLAS",
                    "USE_OPTIMIZED_LAPACK", "BUILD_SHARED_LIBS"}
    option_names.update(f"LAPACKE_BUILD_{name}" for name in
                        ("SINGLE", "DOUBLE", "COMPLEX", "COMPLEX16"))
    return {"label": label, "identity_sha256": record["identity_sha256"],
            "cache_sha256": payload["cache_sha256"],
            "integer_bits": payload["integer_bits"],
            "integer_route": payload["integer_route"],
            "options": {key: payload["options"][key] for key in sorted(option_names)},
            "archives": archives, "symbols": dict(sorted(symbols.items()))}


def routine_sources(routine: dict, graph: dict,
                    source_root: pathlib.Path) -> tuple[list[dict], list[str]]:
    """Collect source identities and direct XBLAS calls, not transitive reachability."""
    sources = []
    calls = set()
    for instance in routine["source_instances"]:
        path = instance["path"]
        libraries = [name for name in ("lapack", "blas") if path in graph[name]]
        sources.append({"path": path, "sha256": instance["sha256"],
                        "libraries": libraries,
                        "inventory_classification": instance["classification"],
                        "required_instance": instance["required"],
                        "selection": "listed" if libraries else "not_in_provider_object_lists"})
        if libraries:
            statements = generate_inventory.logical_statements(
                (source_root / path).read_text(encoding="utf-8"),
                pathlib.Path(path).suffix.lower() in (".f90", ".f95", ".f03", ".f08"))
            for statement in statements:
                match = re.search(r"\bCALL\s+(BLAS_\w+)\s*\(", statement.text, re.I)
                if match:
                    calls.add(match.group(1).lower())
    if not any(source["libraries"] for source in sources):
        raise ValueError(f"Required row has no reviewed source selection: {routine['id']}")
    return sources, sorted(calls)


def reconcile(inventory: dict, graph: dict, providers: list[dict],
              source_root: pathlib.Path) -> dict:
    """Retain every required routine and compare selected sources with exports."""
    rows = []
    mismatches = []
    for routine in inventory["routines"]:
        if "reference_cpu_full" not in routine["required_profiles"]:
            continue
        sources, calls = routine_sources(routine, graph, source_root)
        declarations = sorted({route["name"] for route in routine["interface_routes"]
                               if route["kind"] == "lapacke_declaration"})
        symbols = {}
        for provider in providers:
            expected = any(selected(graph[library][source["path"]], provider["options"])
                           for source in sources for library in source["libraries"])
            definitions = provider["symbols"].get(routine["routine"] + "_", [])
            if expected != bool(definitions):
                mismatches.append({"id": routine["id"], "provider": provider["label"],
                                   "selected": expected, "defined": bool(definitions)})
            symbols[provider["label"]] = {
                "selected_source": expected, "fortran_definitions": definitions,
                "lapacke_definitions": {name: provider["symbols"].get(name, [])
                                         for name in declarations}}
        rows.append({"id": routine["id"], "routine": routine["routine"],
                     "classification": routine["classification"], "sources": sources,
                     "lapacke_declarations": declarations, "direct_xblas_calls": calls,
                     "providers": symbols})
    return {"routines": rows, "source_symbol_mismatches": mismatches}


def dependency_sources(text: str, source_root: pathlib.Path) -> list[str]:
    """Read actual generated CMake object dependency lists without executing them."""
    result = []
    for name, arguments, _ in commands(text):
        if name != "set" or not arguments:
            raise ValueError("Unsupported generated dependency command")
        variable = arguments[0]
        if variable == "CMAKE_DEPENDS_CHECK_Fortran":
            stride = 2
        elif variable == "CMAKE_DEPENDS_DEPENDENCY_FILES":
            stride = 4
        else:
            continue
        values = arguments[1:]
        if len(values) % stride:
            raise ValueError("Malformed generated dependency list")
        result.extend(pathlib.Path(value).relative_to(source_root).as_posix()
                      for value in values[::stride])
    return sorted(set(result))


def build_inputs(paths: list[str], build_root: pathlib.Path,
                 source_root: pathlib.Path) -> tuple[set[str], list[dict]]:
    """Collect generated object-list source paths and exact input hashes."""
    actual = set()
    inputs = []
    for path in paths:
        data = (build_root / path).read_bytes()
        inputs.append({"path": path, "sha256": generate_inventory.sha256(data)})
        actual.update(dependency_sources(data.decode(), source_root))
    return actual, inputs


def check_build_tree(build_root: pathlib.Path, provider: dict, graph: dict,
                     source_root: pathlib.Path) -> dict:
    """Cross-check static source selection against an attested CMake build cache."""
    cache_path = build_root / "CMakeCache.txt"
    if generate_inventory.sha256(cache_path.read_bytes()) != provider["cache_sha256"]:
        raise ValueError("Build cache differs from provider attestation")
    timing = attest_provider.cache_values(cache_path)["TIME_FUNC"]
    if timing not in ("NONE", "INT_CPU_TIME", "EXT_ETIME", "EXT_ETIME_", "INT_ETIME"):
        raise ValueError("Unsupported timing probe result")
    suffix = "64" if provider["integer_bits"] == 64 else ""
    targets = {"blas": [f"BLAS/SRC/CMakeFiles/blas{suffix}_obj.dir/DependInfo.cmake"],
               "lapack": [f"SRC/CMakeFiles/lapack{suffix}_obj.dir/DependInfo.cmake",
                          "SRC/CMakeFiles/mod_files.dir/DependInfo.cmake"],
               "lapacke": [f"LAPACKE/CMakeFiles/lapacke{suffix}_obj.dir/DependInfo.cmake"]}
    result = {"timing_function": timing, "libraries": {}}
    substitutions = {"${SECOND_SRC}": f"INSTALL/second_{timing}.f",
                     "${DSECOND_SRC}": f"INSTALL/dsecnd_{timing}.f"}
    for library, paths in targets.items():
        actual, inputs = build_inputs(paths, build_root, source_root)
        expected = {substitutions.get(path, path) for path, record in graph[library].items()
                    if selected(record, provider["options"])}
        if expected != actual:
            raise ValueError(f"{library} build graph differs: "
                             f"missing={sorted(expected - actual)}, "
                             f"extra={sorted(actual - expected)}")
        result["libraries"][library] = {"inputs": inputs, "sources": sorted(actual),
                                        "source_count": len(actual)}
    return result


def main() -> int:
    """Generate a checked external audit or return a nonzero diagnostic exit."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=pathlib.Path, required=True)
    parser.add_argument("--inventory", type=pathlib.Path, required=True)
    parser.add_argument("--provider-lock", type=pathlib.Path, required=True)
    parser.add_argument("--provider", nargs=3, action="append", required=True,
                        metavar=("LABEL", "ATTESTATION", "PREFIX"))
    parser.add_argument("--nm", default="nm")
    parser.add_argument("--build", nargs=2, action="append", default=[],
                        metavar=("LABEL", "BUILD_ROOT"),
                        help="Also check attested generated object dependency lists.")
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--check", action="store_true",
                        help="Compare deterministic output with an existing audit.")
    args = parser.parse_args()
    try:
        if pathlib.Path(__file__).resolve().parents[2] in args.output.resolve().parents:
            raise ValueError("Audit artifacts must be outside the ASC source tree")
        labels = [entry[0] for entry in args.provider]
        if len(labels) != len(set(labels)) or any(not re.fullmatch(r"[a-z0-9_-]+", name)
                                                 for name in labels):
            raise ValueError("Provider labels must be distinct safe identifiers")
        inventory = json.loads(args.inventory.read_text(encoding="utf-8"))
        graph = source_graph(args.source_root, inventory)
        providers = [provider_symbols(label, pathlib.Path(attestation), pathlib.Path(prefix), args)
                     for label, attestation, prefix in args.provider]
        builds = dict(args.build)
        if len(builds) != len(args.build) or not set(builds) <= set(labels):
            raise ValueError("Build labels must be distinct known provider labels")
        for provider in providers:
            if provider["label"] in builds:
                provider["build_graph_cross_check"] = check_build_tree(
                    pathlib.Path(builds[provider["label"]]), provider, graph, args.source_root)
        result = reconcile(inventory, graph, providers, args.source_root)
        result.update({"schema_version": 1, "purpose": "source_and_archive_review_only",
                       "specification": inventory["specification"],
                       "inventory_sha256": generate_inventory.sha256(args.inventory.read_bytes()),
                       "generator_sha256": generate_inventory.sha256(
                           pathlib.Path(__file__).read_bytes()),
                       "tool_inputs": [
                           {"path": name, "sha256": generate_inventory.sha256(
                               pathlib.Path(__file__).with_name(name).read_bytes())}
                           for name in ("generate_inventory.py", "attest_provider.py",
                                        "validate_coverage.py")],
                       "graph": graph, "providers": providers,
                       "nm_version": subprocess.run([args.nm, "--version"], check=True,
                                                     capture_output=True, text=True).stdout})
        encoded = json.dumps(result, indent=2, sort_keys=True) + "\n"
        if args.check:
            if args.output.read_text(encoding="utf-8") != encoded:
                raise ValueError("Stale build-graph audit")
        else:
            with args.output.open("x", encoding="utf-8") as output:
                output.write(encoded)
        if result["source_symbol_mismatches"]:
            raise ValueError("Unresolved source-selection / symbol disagreement; inspect audit")
        print(f"Audited {len(result['routines'])} required source rows; no ASC capability awarded.")
        return 0
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print(f"Build-graph audit failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
