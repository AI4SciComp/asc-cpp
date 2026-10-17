#!/usr/bin/env python3
"""Inventory pinned LAPACK sources without building or executing upstream code.

The output is a source catalogue, never evidence of ASC implementation. All
documented SRC auxiliary procedures are conservatively required expert entries;
only source-location or module-containment evidence justifies exclusions. The
Fortran reader retains preprocessing alternatives rather than evaluating them.
"""

from __future__ import annotations

import argparse
import collections
import dataclasses
import hashlib
import json
import pathlib
import re
import subprocess
import sys


UPSTREAM_URL = "https://github.com/Reference-LAPACK/lapack.git"
PINNED_COMMIT = "6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca"
PINNED_TAG = "5ebe92156143a341ab7b14bf76560d30093cfc54"
_FORTRAN_SUFFIXES = frozenset((".f", ".for", ".f90", ".f95", ".f03", ".f08"))
_TYPE = (r"DOUBLE\s+PRECISION|DOUBLE\s+COMPLEX|"
         r"(?:INTEGER|REAL|COMPLEX|LOGICAL|CHARACTER)"
         r"(?:\s*\([^)]*\)|\s*\*\s*(?:\([^)]*\)|\d+))?")
_PROCEDURE = re.compile(
    rf"^(?P<prefix>(?:(?:RECURSIVE|PURE|ELEMENTAL|IMPURE|MODULE)\s+|"
    rf"(?:{_TYPE})\s+)*)"
    r"(?P<kind>SUBROUTINE|FUNCTION)\s+(?P<name>[A-Z]\w*)"
    r"\s*(?:\((?P<args>.*?)\))?(?:\s+RESULT\((?P<result>\w+)\))?", re.I)
_DECLARATION = re.compile(rf"^({_TYPE})\s*(.*)$", re.I)


@dataclasses.dataclass
class Statement:
    """One logical Fortran statement with its first physical source line."""

    line: int
    text: str
    conditions: tuple[str, ...]


def sha256(data: bytes) -> str:
    """Return a lowercase content identity."""
    return hashlib.sha256(data).hexdigest()


def verify_blob(data: bytes, expected: str, path: str) -> None:
    """Reject modified input even if Git's index flags hide it from status."""
    blob = b"blob " + str(len(data)).encode("ascii") + b"\0" + data
    if hashlib.sha1(blob).hexdigest() != expected:
        raise ValueError(f"Input bytes do not match pinned Git blob: {path}")


def split_arguments(text: str) -> list[str]:
    """Split commas outside parentheses and quoted Fortran strings."""
    parts = []
    start = 0
    depth = 0
    quote = None
    index = 0
    while index < len(text):
        char = text[index]
        if quote:
            if char == quote:
                if index + 1 < len(text) and text[index + 1] == quote:
                    index += 1
                else:
                    quote = None
        elif char in "\"'":
            quote = char
        elif char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
        elif char == "," and depth == 0:
            parts.append(text[start:index].strip())
            start = index + 1
        index += 1
    if text[start:].strip():
        parts.append(text[start:].strip())
    return parts


def _without_inline_comment(text: str) -> str:
    quote = None
    index = 0
    while index < len(text):
        char = text[index]
        if quote:
            if char == quote:
                if index + 1 < len(text) and text[index + 1] == quote:
                    index += 1
                else:
                    quote = None
        elif char in "\"'":
            quote = char
        elif char == "!":
            return text[:index]
        index += 1
    return text


def logical_statements(text: str, free_form: bool) -> list[Statement]:
    """Read fixed/free form continuations, comments and CPP branch evidence.

    Args:
      text: UTF-8-decoded Fortran source.
      free_form: Whether the file suffix selects free rather than fixed form.

    Returns:
      Logical statements in source order. Conditional branches are all retained.
    """
    result = []
    pending = ""
    first_line = 0
    pending_conditions = ()
    conditions = []
    continued = False
    for number, line in enumerate(text.splitlines(), 1):
        stripped = line.lstrip()
        if stripped.startswith("#"):
            directive = stripped[1:].strip()
            if re.match(r"if(?:def|ndef)?\b", directive):
                conditions.append(directive)
            elif directive.startswith(("else", "elif")) and conditions:
                conditions[-1] += " -> " + directive
            elif directive.startswith("endif") and conditions:
                conditions.pop()
            continue
        if not stripped:
            continue
        if free_form:
            if stripped.startswith("!"):
                continue
            body = _without_inline_comment(stripped).rstrip()
            is_continuation = continued
            if is_continuation:
                body = body.lstrip("&").lstrip()
            continued = body.endswith("&")
            body = body[:-1].rstrip() if continued else body
        else:
            if line[0] in "cC*!":
                continue
            expanded = line.expandtabs(8)
            body = _without_inline_comment(expanded[6:72]).rstrip()
            is_continuation = len(expanded) > 5 and expanded[5] not in " 0"
        if not body:
            continue
        if is_continuation and pending:
            pending += " " + body
        else:
            if pending:
                result.append(Statement(first_line, pending.strip(),
                                        pending_conditions))
            pending = body
            first_line = number
            pending_conditions = tuple(conditions)
    if pending:
        result.append(Statement(first_line, pending.strip(), pending_conditions))
    return result


def _canonical_type(fortran_type: str, body: str) -> str:
    compact = re.sub(r"\s+", "", fortran_type.upper())
    if compact.startswith("CHARACTER"):
        return "character"
    if compact.startswith("INTEGER"):
        return "fortran_integer" + ("_" + compact if compact != "INTEGER" else "")
    if compact.startswith("LOGICAL"):
        return "fortran_logical"
    aliases = dict(re.findall(r"\b(\w+)\s*=>\s*(sp|dp)\b", body.lower()))
    aliases.update(re.findall(r"\b(\w+)\s*=\s*(real32|real64)\b", body.lower()))
    if compact.startswith(("REAL", "COMPLEX")):
        base = "complex" if compact.startswith("COMPLEX") else "real"
        if compact == "COMPLEX*8":
            return "c64"
        kind_match = re.search(r"\((?:KIND=)?(\w+)\)|\*(\d+)", compact)
        kind = next(filter(None, kind_match.groups()), "") if kind_match else ""
        kind = aliases.get(kind.lower(), kind.lower())
        if not kind or kind in ("sp", "4", "real32"):
            return "c64" if base == "complex" else "f32"
        if kind in ("dp", "8", "real64"):
            return "c128" if base == "complex" else "f64"
        if kind == "16" and base == "complex":
            return "c128"
        return "unresolved:" + compact
    return {"DOUBLEPRECISION": "f64", "DOUBLECOMPLEX": "c128"}.get(
        compact, "unresolved:" + compact)


def _argument_contracts(statement: Statement, body: list[Statement],
                        documentation: str, arguments: list[str],
                        result_name: str | None) -> tuple[list[dict], dict | None]:
    declarations = {}
    intents = {}
    external = set()
    body_text = "\n".join(item.text for item in body)
    for intent, name in re.findall(
            r"[@\\]param\s*\[([^]]+)\]\s+(\w+)", documentation, re.I):
        intents[name.lower()] = intent.lower().replace(" ", "")
    for item in body:
        match = _DECLARATION.match(item.text)
        if match and not _PROCEDURE.match(item.text):
            fortran_type, remainder = match.groups()
            attributes = ""
            if "::" in remainder:
                attributes, remainder = remainder.split("::", 1)
            declared_intent = re.search(r"INTENT\s*\(([^)]+)\)", attributes, re.I)
            for token in split_arguments(remainder):
                name_match = re.match(r"([A-Z]\w*)\s*(\(.*\))?", token, re.I)
                if not name_match:
                    continue
                name, dimensions = name_match.groups()
                declarations[name.lower()] = {
                    "fortran_type": " ".join(fortran_type.upper().split()),
                    "type": _canonical_type(fortran_type, body_text),
                    "dimensions": dimensions or None,
                    "declaration_line": item.line,
                }
                if declared_intent:
                    intent = declared_intent.group(1).lower().replace(" ", "")
                    intents[name.lower()] = "in,out" if intent == "inout" else intent
                if re.search(r"\bEXTERNAL\b", attributes, re.I):
                    external.add(name.lower())
        if re.match(r"EXTERNAL\b", item.text, re.I):
            external.update(name.strip().lower() for name in
                            item.text.split(None, 1)[1].replace("::", "").split(","))
    records = []
    for argument in arguments:
        name = argument.lower()
        record = {"name": name, **declarations.get(name, {"type": "unresolved"}),
                  "intent": intents.get(name, "unspecified"),
                  "external_procedure": name in external}
        if record["external_procedure"] and record["type"] == "unresolved":
            record["type"] = "external_procedure"
        doc_match = re.search(r"[@\\]param\s*(?:\[[^]]+\])?\s+" + re.escape(name)
                              + r"\b(.*?)(?=[@\\]param\b|\Z)", documentation,
                              re.I | re.S)
        if doc_match:
            block = doc_match.group(1)
            if record["type"] == "character":
                record["documented_literal_candidates"] = sorted(set(re.findall(
                    r"['\"]([A-Za-z])['\"]", block)))
        records.append(record)
    result = None
    if result_name:
        result = declarations.get(result_name.lower())
        if result is None:
            function_type = _DECLARATION.match(statement.text)
            if function_type:
                fortran_type = function_type.group(1)
                result = {"fortran_type": fortran_type.upper(),
                          "type": _canonical_type(fortran_type, body_text),
                          "declaration_line": statement.line}
    return records, result


def parse_fortran(text: str, path: str) -> tuple[list[dict], list[dict]]:
    """Discover every external/module/internal procedure and nonprocedure unit.

    Documentation is recorded as line references and argument direction facts;
    upstream prose and implementation statements are not copied into output.
    """
    statements = logical_statements(text, pathlib.PurePosixPath(path).suffix.lower()
                                    not in (".f", ".for"))
    stack = []
    module = None
    procedures = []
    units = []
    lines = text.splitlines()
    previous_end = 0
    for index, statement in enumerate(statements):
        source = statement.text
        module_match = re.match(r"MODULE\s+(?!PROCEDURE\b)(\w+)\s*$", source, re.I)
        if module_match:
            module = module_match.group(1).lower()
            units.append({"kind": "module", "name": module,
                          "path": path, "line": statement.line})
        if re.match(r"END\s*MODULE\b", source, re.I):
            module = None
        program_match = re.match(r"PROGRAM\s+(\w+)", source, re.I)
        if program_match:
            units.append({"kind": "program", "name": program_match.group(1).lower(),
                          "path": path, "line": statement.line})
        if re.match(r"END\s*(?:(?:SUBROUTINE|FUNCTION)(?:\s+\w+)?)?\s*$", source, re.I):
            if stack:
                stack.pop()
            previous_end = statement.line
            continue
        match = _PROCEDURE.match(source)
        if not match:
            continue
        name = match.group("name").lower()
        arguments = split_arguments(match.group("args") or "")
        body_end = len(statements)
        for following in range(index + 1, len(statements)):
            if (_PROCEDURE.match(statements[following].text) or
                    re.match(r"END\s*(?:(?:SUBROUTINE|FUNCTION)(?:\s+\w+)?)?\s*$",
                             statements[following].text, re.I)):
                body_end = following
                break
        docs = "\n".join(lines[previous_end:statement.line - 1])
        body = statements[index + 1:body_end]
        argument_records, return_type = _argument_contracts(
            statement, body, docs, arguments,
            (match.group("result") or name) if match.group("kind").lower() == "function"
            else None)
        groups = sorted(set(re.findall(r"[@\\]ingroup\s+(\w+)", docs)))
        category_text = "\n".join(lines[previous_end:
                                     statements[body_end].line if body_end < len(statements)
                                     else len(lines)])
        categories = sorted(set(item.lower() for item in re.findall(
            r"LAPACK\s+(driver|computational|auxiliary|test|testing)\s+routine",
            category_text, re.I)))
        scopes = ([module] if module else []) + stack
        procedures.append({
            "routine": name,
            "scope": scopes,
            "kind": match.group("kind").lower(),
            "path": path,
            "line": statement.line,
            "declaration": " ".join(source.split()),
            "preprocessor_conditions": list(statement.conditions),
            "arguments": argument_records,
            "return_type": return_type,
            "documentation": {"path": path, "start_line": previous_end + 1,
                              "declaration_line": statement.line, "groups": groups,
                              "upstream_categories": categories},
        })
        stack.append(name)
    return procedures, units


def parse_documentation_groups(text: str) -> list[dict]:
    """Read the upstream group hierarchy, including absent routine placeholders."""
    groups = []
    stack = []
    last = None
    for number, line in enumerate(text.splitlines(), 1):
        match = re.search(r"[@\\]defgroup\s+(\w+)\s+(.*)", line)
        if match:
            name, description = match.groups()
            groups.append({"id": name, "parents": list(stack), "line": number,
                           "absent_placeholder": "not available" in description.lower()})
            last = name
        if "@{" in line or "\\{" in line:
            if last:
                stack.append(last)
        if ("@}" in line or "\\}" in line) and stack:
            stack.pop()
    return groups


def parse_build_references(text: str, path: str,
                           source_paths: set[str]) -> list[dict]:
    """Extract literal CMake/Make source-list facts without evaluating either."""
    references = []
    conditions = []
    variable = None
    parent = pathlib.PurePosixPath(path).parent
    for number, line in enumerate(text.splitlines(), 1):
        line = line.split("#", 1)[0]
        condition = re.match(r"\s*(if|elseif|else|endif)\s*\((.*?)\)", line, re.I)
        if condition:
            operation, expression = condition.groups()
            if operation.lower() == "if":
                conditions.append(expression.strip())
            elif operation.lower() in ("elseif", "else") and conditions:
                conditions[-1] += " -> " + operation + "(" + expression + ")"
            elif conditions:
                conditions.pop()
        assignment = re.match(r"\s*(?:set\s*\(|list\s*\(APPEND\s+)(\w+)", line, re.I)
        make_assignment = re.match(r"\s*(\w+)\s*[:+?]?=", line)
        if assignment or make_assignment:
            variable = (assignment or make_assignment).group(1)
        for match in re.finditer(r"(?<![\w/])([\w./+-]+\.(?:f90|f95|for|f|o))\b", line, re.I):
            token = match.group(1)
            candidates = [token]
            if token.endswith(".o"):
                candidates = [token[:-2] + suffix for suffix in (".f", ".F", ".f90", ".F90", ".c")]
            resolved = []
            for candidate in candidates:
                parts = []
                for part in (parent / candidate).parts:
                    if part == ".." and parts:
                        parts.pop()
                    elif part != ".":
                        parts.append(part)
                normalized = "/".join(parts)
                if normalized in source_paths:
                    resolved.append(normalized)
            references.append({"build_path": path, "line": number,
                               "variable": variable, "token": token,
                               "conditions": list(conditions), "source_paths": resolved,
                               "resolution": "literal_source" if resolved else "nonliteral_or_generated_build_token"})
        if assignment and ")" in line:
            variable = None
    return references


def _classify(instance: dict) -> tuple[str, bool, str]:
    path = instance["path"]
    categories = instance["documentation"]["upstream_categories"]
    if instance["scope"]:
        return "internal_dependency", False, "Fortran module/contained procedure; no external entry ABI."
    if path.startswith(("TESTING/", "CBLAS/testing/", "BLAS/TESTING/")):
        return "test_only", False, "Upstream test or test-matrix-generator source subtree."
    if path.startswith("BLAS/SRC/"):
        return "internal_dependency", False, "Reference BLAS provider dependency; existing ASC BLAS scope is separate."
    if path.startswith("CBLAS/src/"):
        return "internal_dependency", False, "CBLAS Fortran interoperability wrapper, not a LAPACK numerical operation."
    if "/DEPRECATED/" in path or path.startswith("DEPRECATED/"):
        return "deprecated_compatibility", True, "Pinned deprecated entry retained in full denominator."
    if path.startswith("SRC/"):
        if "driver" in categories:
            return "public_driver", True, "Source identifies a LAPACK driver routine."
        if "computational" in categories:
            return "public_computational", True, "Source identifies a LAPACK computational routine."
        if "auxiliary" in categories or instance["documentation"]["groups"]:
            return "expert_auxiliary", True, "Documented SRC expert entry conservatively retained; not excluded as internal."
        return "unresolved", False, "SRC entry lacks recognized classification evidence; review required."
    if path.startswith("INSTALL/"):
        if instance["routine"] in ("slamch", "dlamch", "lsame", "ilaver",
                                    "sroundup_lwork", "droundup_lwork"):
            return "expert_auxiliary", True, "INSTALL entry is explicitly included by SRC/CMakeLists.txt."
        return "build_support", False, "Upstream INSTALL platform probe or timing support implementation."
    return "unresolved", False, "Unrecognized source subtree; review required."


def _git(source_root: pathlib.Path, *arguments: str) -> str:
    return subprocess.run(["git", "-C", str(source_root), *arguments],
                          check=True, capture_output=True, text=True).stdout.strip()


def verify_source(source_root: pathlib.Path, commit: str) -> dict:
    """Verify the immutable clean Git tree and expected annotated tag identities.

    Raises:
      ValueError: The request, checkout, tag or working tree is inconsistent.
      subprocess.CalledProcessError: A checked read-only Git operation fails.
    """
    if commit != PINNED_COMMIT:
        raise ValueError("Only the approved Reference-LAPACK 3.12.1 commit is supported")
    if _git(source_root, "rev-parse", "HEAD") != commit:
        raise ValueError("Source HEAD does not match requested pinned commit")
    if _git(source_root, "status", "--porcelain", "--untracked-files=all"):
        raise ValueError("Source working tree must be pristine, including untracked paths")
    tag = _git(source_root, "rev-parse", "v3.12.1")
    if tag != PINNED_TAG or _git(source_root, "rev-parse", "v3.12.1^{}") != commit:
        raise ValueError("Annotated tag identity or peeled commit mismatch")
    tag_text = _git(source_root, "cat-file", "-p", tag)
    signature = "present_not_verified" if "-----BEGIN PGP SIGNATURE-----" in tag_text else "unsigned"
    return {"project": "Reference-LAPACK", "version": "3.12.1",
            "upstream_url": UPSTREAM_URL, "commit": commit,
            "tree": _git(source_root, "rev-parse", "HEAD^{tree}"),
            "tag": "v3.12.1", "tag_object": tag, "tag_signature": signature}


def generate_inventory(source_root: pathlib.Path, commit: str) -> dict:
    """Generate deterministic declarations, classifications and discovery evidence."""
    specification = verify_source(source_root, commit)
    blobs = {}
    for line in _git(source_root, "ls-tree", "-r", commit).splitlines():
        metadata, path = line.split("\t", 1)
        blobs[path] = metadata.split()[2]
    paths = list(blobs)
    fortran_paths = {path for path in paths
                     if pathlib.PurePosixPath(path).suffix.lower() in _FORTRAN_SUFFIXES}
    build_paths = {path for path in paths if pathlib.PurePosixPath(path).name.lower()
                   in ("cmakelists.txt", "makefile", "make.inc.example") or path.endswith(".cmake")}
    docs_path = "DOCS/groups-usr.dox"
    header_paths = {path for path in paths if path.startswith("LAPACKE/include/") and path.endswith(".h")}
    inputs = []
    contents = {}
    for path in sorted(fortran_paths | build_paths | header_paths | {docs_path}):
        data = (source_root / path).read_bytes()
        verify_blob(data, blobs[path], path)
        contents[path] = data.decode("utf-8", errors="strict")
        inputs.append({"path": path, "sha256": sha256(data)})
    hashes = {item["path"]: item["sha256"] for item in inputs}
    groups = parse_documentation_groups(contents[docs_path])
    groups_by_id = {group["id"]: group for group in groups}
    build_references = []
    for path in sorted(build_paths):
        build_references.extend(parse_build_references(contents[path], path, set(paths)))
    by_source = collections.defaultdict(list)
    for reference_id, reference in enumerate(build_references):
        reference["id"] = reference_id
        for path in reference["source_paths"]:
            by_source[path].append(reference)
    interfaces = collections.defaultdict(list)
    for path in sorted(header_paths):
        for match in re.finditer(r"\bLAPACKE_(\w+)\s*\(", contents[path]):
            line_start = contents[path].rfind("\n", 0, match.start()) + 1
            if contents[path][line_start:match.start()].lstrip().startswith("#"):
                continue
            name = match.group(1).lower()
            numerical_name = name
            if numerical_name.endswith("_64"):
                numerical_name = numerical_name[:-3]
            if numerical_name.endswith("_work"):
                numerical_name = numerical_name[:-5]
            interfaces[numerical_name].append({"kind": "lapacke_declaration",
                                               "name": "LAPACKE_" + name,
                                               "path": path,
                                               "line": contents[path][:match.start()].count("\n") + 1})
    by_routine = collections.defaultdict(list)
    units = []
    source_without_procedures = []
    for path in sorted(fortran_paths):
        instances, discovered_units = parse_fortran(contents[path], path)
        units.extend(discovered_units)
        if not instances:
            source_without_procedures.append(path)
        for instance in instances:
            instance["sha256"] = hashes[path]
            instance["build_references"] = by_source[path]
            classification, required, reason = _classify(instance)
            instance["classification"] = classification
            instance["classification_reason"] = reason
            instance["required"] = required
            identity = ".".join(instance["scope"] + [instance["routine"]])
            by_routine[identity].append(instance)
    routines = []
    unresolved = []
    for identity, instances in sorted(by_routine.items()):
        instances.sort(key=lambda item: (not item["path"].startswith("SRC/"),
                                        not item["required"],
                                        "/VARIANTS/" in item["path"],
                                        item["path"], item["line"]))
        canonical = instances[0]
        required = any(instance["required"] for instance in instances)
        routine_id = "lapack." + identity
        argument_types = sorted({argument["type"] for argument in canonical["arguments"]})
        if canonical["return_type"]:
            argument_types.append(canonical["return_type"]["type"])
        floating_types = sorted(set(argument_types) & {"f32", "f64", "c64", "c128"})
        family = canonical["documentation"]["groups"]
        conditions = set()
        for instance in instances:
            for reference in instance["build_references"]:
                conditions.update(reference["conditions"])
                if reference["variable"] in ("SXLASRC", "DXLASRC", "CXLASRC", "ZXLASRC"):
                    conditions.add("USE_XBLAS")
        row = {"id": routine_id, "routine": canonical["routine"],
               "classification": canonical["classification"],
               "classification_reason": canonical["classification_reason"],
               "required_profiles": ["reference_cpu_full"] if required else [],
               "deprecated": any("/DEPRECATED/" in item["path"] for item in instances),
               "precision": floating_types, "input_output_types": canonical["arguments"],
               "return_type": canonical["return_type"],
               "family": family or ["unclassified"],
               "documentation_group_ids": [group for group in family if group in groups_by_id],
               "optional_build_conditions": sorted(conditions),
               "source_instances": instances,
               "legal_mode_contract_review": "pending_reviewed_ASC_mapping",
               "interface_routes": [{"kind": "fortran_source", "qualified_name": identity}]
                                   + interfaces.get(canonical["routine"], [])}
        routines.append(row)
        if any(instance["classification"] == "unresolved" for instance in instances):
            unresolved.append({"id": routine_id, "reason": "unresolved_source_classification"})
        if required:
            for argument in canonical["arguments"]:
                if argument["type"].startswith("unresolved"):
                    unresolved.append({"id": routine_id, "argument": argument["name"],
                                       "reason": "unresolved_argument_type"})
        for instance in instances:
            instance["build_reference_ids"] = [reference["id"] for reference in
                                               instance.pop("build_references")]
            arguments = instance.pop("arguments")
            instance["argument_contracts_sha256"] = sha256(
                json.dumps(arguments, sort_keys=True).encode())
            if arguments != row["input_output_types"]:
                instance["variant_argument_contracts"] = arguments
            instance.pop("return_type")
    implemented_names = {row["routine"] for row in routines if not row["source_instances"][0]["scope"]}
    unmatched_interfaces = sorted(set(interfaces) - implemented_names)
    summary = {"fortran_source_files": len(fortran_paths),
               "procedure_source_instances": sum(len(row["source_instances"]) for row in routines),
               "unique_qualified_procedures": len(routines),
               "required_routines": sum(bool(row["required_profiles"]) for row in routines),
               "classification_counts": dict(sorted(collections.Counter(
                   row["classification"] for row in routines).items())),
               "unresolved_items": len(unresolved)}
    generator_bytes = pathlib.Path(__file__).read_bytes()
    return {"schema_version": 1,
            "generator": {"path": "tools/lapack/generate_inventory.py",
                          "sha256": sha256(generator_bytes),
                          "command": "python3 tools/lapack/generate_inventory.py --source-root <verified-source> --commit "
                                     + commit + " --output docs/contracts/lapack-upstream-inventory.json"},
            "specification": specification, "source_inputs": inputs,
            "source_input_manifest_sha256": sha256(json.dumps(inputs, sort_keys=True).encode()),
            "routines": routines, "source_units": units,
            "source_files_without_procedures": source_without_procedures,
            "documentation_groups": groups,
            "build_references": build_references,
            "lapacke_without_fortran_source": unmatched_interfaces,
            "unresolved_classifications": unresolved, "summary": summary}


def provider_lock(source_root: pathlib.Path, inventory: dict) -> dict:
    """Return a source-only lock, without asserting a provider build exists."""
    licenses = []
    for path in ("LICENSE", "LAPACKE/LICENSE"):
        licenses.append({"path": path, "sha256": sha256((source_root / path).read_bytes())})
    return {"schema_version": 1, "specification": inventory["specification"],
            "acquisition": {"method": "git_from_authoritative_https_remote",
                            "archive_url": None, "archive_sha256": None,
                            "authentication": "HTTPS transport; upstream annotated tag is unsigned"},
            "verification": {"method": "exact HEAD, tree, tag object and peeled commit; pristine git status; each input byte sequence matches pinned Git blob",
                             "source_input_manifest_sha256": inventory["source_input_manifest_sha256"]},
            "licenses_and_notices": licenses,
            "license_expression": "LicenseRef-LAPACK-3.12.1-Root",
            "license_observations": ["Root BSD-style terms contain an additional intellectual-property non-assurance clause",
                                     "LAPACKE/LICENSE contains BSD-3-Clause terms; redistribution remains owner-reviewed"],
            "redistribution_approval": "pending_owner_review",
            "patches": [], "provider_builds": [],
            "build_options": {"status": "not_built", "required_precisions": ["S", "D", "C", "Z"],
                              "extra_precision": "USE_XBLAS retained in required inventory",
                              "deprecated": "BUILD_DEPRECATED retained in required inventory"},
            "metadata_discrepancies": ["Pinned v3.12.1 CMakeLists.txt declares LAPACK_PATCH_VERSION 0"]}


def main() -> int:
    """Generate files or check existing deterministic output; fail on drift."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=pathlib.Path, required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--lock-output", type=pathlib.Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    try:
        inventory = generate_inventory(args.source_root, args.commit)
        outputs = [(args.output, inventory)]
        if args.lock_output:
            outputs.append((args.lock_output, provider_lock(args.source_root, inventory)))
        for path, data in outputs:
            serialized = json.dumps(data, indent=2, sort_keys=True) + "\n"
            if args.check:
                if path.read_text(encoding="utf-8") != serialized:
                    raise ValueError(f"Generated output is stale: {path}")
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(serialized, encoding="utf-8")
        print(json.dumps(inventory["summary"], sort_keys=True))
        return 0
    except (OSError, UnicodeError, ValueError, subprocess.CalledProcessError) as error:
        print(f"inventory error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
