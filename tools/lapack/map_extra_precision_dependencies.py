# Copyright 2026 AI4SciComp contributors
# SPDX-License-Identifier: Apache-2.0
"""Map absent pinned definitions and external XBLAS bridge requirements.

The retained source/symbol audit is input evidence, not a command to execute.
Only the selected missing-source dependencies are revalidated. Neither an
archive definition nor this conservative declaration closure proves numerical
acceptance. The candidate dependency is inspected, never built or adopted.
"""
import argparse
import hashlib
import json
import pathlib
import re
import tarfile

import generate_inventory


def digest(path):
    """Return the identity of an inspected input."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition, message):
    """Reject stale or incomplete evidence even when Python assertions are off."""
    if not condition:
        raise ValueError(message)


def declared_dependencies(text):
    """Read fixed-form CALL and EXTERNAL edges, retaining native call lines."""
    dependencies = set()
    calls = []
    for statement in generate_inventory.logical_statements(text, False):
        match = re.search(r'\bCALL\s+(\w+)\s*\(', statement.text, re.I)
        if match:
            callee = match.group(1).lower()
            dependencies.add(callee)
            calls.append({'callee': callee, 'line': statement.line})
        external = re.match(r'\s*EXTERNAL\s+(.*)', statement.text, re.I)
        if external:
            dependencies.update(re.findall(r'\w+', external.group(1).lower()))
    return dependencies, calls


def closure(edges, name):
    """Separate absent-source traversal, XBLAS helpers and provider boundaries."""
    visited = set()
    pending = [name]
    helpers = set()
    present = set()
    while pending:
        current = pending.pop()
        if current in visited:
            continue
        visited.add(current)
        for callee in edges[current]:
            if callee in edges:
                pending.append(callee)
            elif callee.startswith('blas_'):
                helpers.add(callee)
            else:
                present.add(callee)
    return visited, helpers, present


def build_map(audit_path, inventory_path, source_root, xblas_archive):
    """Return declared/called dependency closure, without compiling sources."""
    audit = json.loads(audit_path.read_text())
    inventory = json.loads(inventory_path.read_text())
    require(
        digest(inventory_path) == audit['inventory_sha256'],
        'Inventory identity changed')
    require(
        digest(xblas_archive) ==
        'b5fe7c71c2da1ed9bcdc5784a12c5fa9fb417577513fe8a38de5de0007f7aaa1',
        'Candidate XBLAS archive identity changed')
    records = {row['id']: row for row in inventory['routines']}
    missing = {
        row['routine']: row
        for row in audit['routines']
        if not row['providers']['lp64']['fortran_definitions']
    }
    require(len(missing) == 130, 'Expected exactly 130 retained absent rows')
    missing_ilp64 = {
        row['routine']
        for row in audit['routines']
        if not row['providers']['ilp64']['fortran_definitions']
    }
    require(set(missing) == missing_ilp64, 'Actual ABI absence sets differ')
    edges = {}
    source_records = {}
    for name, row in missing.items():
        selected = [
            item for item in row['sources'] if item['required_instance']
        ]
        require(len(selected) == 1, 'Ambiguous required source for ' + name)
        source = selected[0]
        path = source_root / source['path']
        require(
            digest(path) == source['sha256'],
            'Source identity changed: ' + str(path))
        dependencies, calls = declared_dependencies(path.read_text())
        edges[name] = dependencies
        source_records[name] = {
            'id': row['id'],
            'source': source['path'],
            'source_sha256': source['sha256'],
            'classification': row['classification'],
            'precision': records[row['id']]['precision'],
            'build_selection': audit['graph']['lapack'][source['path']],
            'direct_calls': calls,
            'declared_or_called_dependencies': sorted(dependencies),
            'direct_xblas_helpers': row['direct_xblas_calls'],
            'missing_definition_in': ['lp64', 'ilp64'],
            'absence_kind': 'source_present_USE_XBLAS_OFF_not_ASC_binding',
        }
    all_helpers = set()
    for name, row in source_records.items():
        visited, helpers, present = closure(edges, name)
        row['absent_source_closure'] = sorted(visited)
        row['transitive_xblas_helpers'] = sorted(helpers)
        row['existing_provider_boundary_dependencies'] = sorted(present)
        row['decision_scope'] = 'extra_precision_dependency_and_abi' if helpers else 'enable_pinned_source_build_selection'
        all_helpers.update(helpers)
    require(len(all_helpers) == 28, 'External helper set changed')
    for provider in audit['providers']:
        require(provider['options']['USE_XBLAS'] == 'OFF',
                'Audit build selection changed')
        for row in source_records.values():
            for callee in row['existing_provider_boundary_dependencies']:
                require(
                    callee + '_' in provider['symbols'],
                    'Missing provider boundary: ' + provider['label'] + ':' +
                    callee)
    helper_records = []
    with tarfile.open(xblas_archive) as archive:
        license_bytes = archive.extractfile('xblas-1.0.248/LICENSE').read()
        names = archive.getnames()
        for helper in sorted(all_helpers):
            matches = [
                path for path in names
                if path.lower().endswith('/' + helper + '-f2c.c')
            ]
            require(len(matches) == 1, 'Missing or duplicate bridge: ' + helper)
            data = archive.extractfile(matches[0]).read()
            require(
                re.search(rb'\bint\s*\*', data) is not None,
                'C int bridge assumption changed: ' + helper)
            helper_records.append({
                'fortran_symbol':
                    helper + '_',
                'bridge_source':
                    matches[0],
                'sha256':
                    hashlib.sha256(data).hexdigest(),
                'integer_signature':
                    'C int pointers; no native int64 route',
                'required_by': [
                    row['id']
                    for row in source_records.values()
                    if helper in row['transitive_xblas_helpers']
                ]
            })
    output = {
        'schema_version':
            1,
        'required_denominator':
            2113,
        'source_audit': {
            'path': audit_path.name,
            'sha256': digest(audit_path),
            'inventory_sha256': digest(inventory_path)
        },
        'scope':
            'Conservative declared/called source closure within the 130 absent rows; existing provider definitions are explicit boundary dependencies, not new link or numerical evidence.',
        'provider_identities': {
            p['label']: p['identity_sha256'] for p in audit['providers']
        },
        'candidate_dependency': {
            'name': 'XBLAS',
            'version': '1.0.248',
            'url': 'https://www.netlib.org/xblas/xblas.tar.gz',
            'archive_sha256': digest(xblas_archive),
            'license_member': 'xblas-1.0.248/LICENSE',
            'license_sha256': hashlib.sha256(license_bytes).hexdigest(),
            'adopted': False,
            'built': False
        },
        'counts': {
            'absent_required_rows':
                130,
            'reaches_xblas':
                sum(
                    bool(r['transitive_xblas_helpers'])
                    for r in source_records.values()),
            'no_xblas_call_in_closure':
                sum(not r['transitive_xblas_helpers']
                    for r in source_records.values()),
            'distinct_external_helpers':
                28
        },
        'rows':
            list(source_records.values()),
        'external_helpers':
            helper_records,
    }
    return output


def main():
    """Regenerate or check the compact maintained review map."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--audit', required=True, type=pathlib.Path)
    parser.add_argument('--inventory', required=True, type=pathlib.Path)
    parser.add_argument('--source-root', required=True, type=pathlib.Path)
    parser.add_argument('--xblas-archive', required=True, type=pathlib.Path)
    parser.add_argument('--output', required=True, type=pathlib.Path)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    result = build_map(args.audit, args.inventory, args.source_root,
                       args.xblas_archive)
    content = json.dumps(result, indent=2) + '\n'
    if args.check:
        if args.output.read_text() != content:
            raise SystemExit(
                'Extra-precision dependency map differs from its inputs')
    else:
        with args.output.open('x') as stream:
            stream.write(content)
    print(json.dumps(result['counts'], indent=2))


if __name__ == '__main__':
    main()
