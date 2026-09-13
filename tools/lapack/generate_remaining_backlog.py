# Copyright 2026 AI4SciComp contributors
# SPDX-License-Identifier: Apache-2.0
"""Generate the finite Reference backlog without granting execution credit."""

import argparse
import collections
import hashlib
import json
import pathlib

import validate_coverage


def owner(family):
    """Assign programme ownership; general auxiliaries stay in P09 closure."""
    if family in {'gesv_mixed', 'posv_mixed', 'gedmd'}:
        return 'P09'
    if family in {
            'getrf', 'getrf2', 'getf2', 'getri', 'getrs', 'gesv', 'gesvx',
            'gecon', 'gerfs', 'geequ', 'geequb', 'getc2', 'gesc2'
    }:
        return 'P04'
    if family in {'gebrd', 'gebd2', 'gbbrd', 'labrd', 'lasv2', 'lapll'}:
        return 'P07'
    if family.startswith(
        ('gesvd', 'gesdd', 'gesvj', 'gejsv', 'gsvj', 'ggsv', 'bbcsd', 'bds',
         'lasd', 'lasq', 'lals', 'orcsd', 'uncsd')):
        return 'P07'
    if family.startswith(
        ('geqr', 'geql', 'geqp', 'gerq', 'gelq', 'gel', 'gemq', 'geml', 'gets',
         'ggqr', 'ggrq', 'ggglm', 'gglse', 'org', 'orm', 'ung', 'unm', 'larf',
         'larz', 'tzrz', 'tplq', 'tpqr', 'tpml', 'tpmq')):
        return 'P06'
    if family.startswith(
        ('geev', 'gees', 'gebal', 'gebak', 'gehr', 'gehd', 'ggev', 'gges',
         'ggbal', 'ggbak', 'ggh', 'hseq', 'hsein', 'hgeq', 'trev', 'trex',
         'trsen', 'trsna', 'trsyl', 'tgs', 'tge', 'ste', 'steb', 'stem')):
        return 'P08'
    if family.startswith(
        ('heev', 'hegv', 'hegs', 'hetrd', 'hetd', 'hbev', 'hbg', 'hbtrd',
         'hb2st', 'hpev', 'hpg', 'hptrd', 'syev', 'sygv', 'sygs', 'sytrd',
         'sytd', 'sbev', 'sbg', 'sbtrd', 'sb2st', 'spev', 'spg', 'sptrd')):
        return 'P08'
    if family.startswith(
        ('gb', 'gt', 'pt', 'po', 'pp', 'pb', 'pf', 'sy', 'he', 'hp', 'sp',
         'trcon', 'trrfs', 'trtri', 'trti', 'trtrs', 'tpcon', 'tprfs', 'tptri',
         'tptrs', 'tbcon', 'tbrfs', 'tbtrs')):
        return 'P05'
    return 'P09'


def generate(inventory, mapping, dependency_map):
    """Account for each required row once, exposing unresolved modes honestly."""
    rows = {row['id']: row for row in mapping['routines']}
    blocked = {row['id']: row for row in dependency_map['rows']}
    groups = collections.defaultdict(list)
    required = [
        row for row in inventory['routines']
        if 'reference_cpu_full' in row['required_profiles']
    ]
    validate_coverage.require(
        len({row['id'] for row in required}) == len(required),
        'Duplicate required inventory row')
    for row in required:
        validate_coverage.require(row['id'] in rows,
                                  'Required mapping row missing')
        family = '+'.join(row['family']) or row['routine']
        groups[family].append(row)
    tasks = []
    for family, members in sorted(groups.items()):
        ids = sorted(row['id'] for row in members)
        package = owner(family)
        selected = [rows[identifier] for identifier in ids]
        missing = [identifier for identifier in ids if identifier in blocked]
        registered = [
            row for row in selected
            if row['implementations']['reference_cpu']['state'] != 'not_started'
        ]
        sources = sorted({
            source['path']
            for row in members
            for source in row['source_instances']
            if source['required']
        })
        files = sorted({
            item['path']
            for row in selected
            for item in row['implementation_artifacts']
        })
        modes = {
            row['id']: [mode['id'] for mode in row['contract']['mode_cases']]
            for row in selected
        }
        reviews = sorted(
            {row['contract'].get('documentation', '') for row in selected} -
            {''})
        if missing:
            action = 'Review the bound extra-precision/source-selection decision for these rows; retain source and ABI contracts while provider admission is pending.'
        elif registered:
            action = 'Reconcile this family\'s existing implementation/review against its exact mode cases; add the first missing required class/profile evidence or repair a concrete first-party defect.'
        else:
            action = 'Review the listed pinned declarations, arguments and legal modes; write their checked ASC contract and adapter, then execute ordinary, failure, workspace and public-consumer tests in both actual ABIs.'
        task = {
            'task_id':
                package + '.required.' + family,
            'owner':
                package,
            'upstream_rows':
                ids,
            'prerequisite_ids': [
                'P01.workspace_report_pivots', 'P04.provider_component'
            ],
            'upstream_source_files':
                sources,
            'source_api_files':
                files,
            'route_states': {
                row['id']: row['implementations']['reference_cpu']['state']
                for row in selected
            },
            'next_action':
                action,
            'required_mode_cases':
                modes,
            'unreviewed_mode_rows': [
                identifier for identifier in ids if not modes[identifier]
            ],
            'required_tests': [
                'exact_contract_modes', 'independent_mathematics',
                'failure_reports', 'workspace_aliasing',
                'public_install_relocation', 'actual_abis', 'required_platforms'
            ],
            'reuse_candidates':
                reviews,
            'blocked_rows':
                missing,
            'blocker_id':
                'P09.extra_precision_provider_decision' if missing else None,
            'integrated_revision':
                'See route-specific review/source artifacts; no whole-tree execution is inferred.'
                if registered else None,
            'execution_progress':
                'partial_source_and_review_exist'
                if registered else 'not_executed',
            'numerical_result':
                'Use exact route review; no blanket pass or failure is inferred from registration.',
            'profile_admission':
                'GNU/Linux/static ABI prerequisites exist; routine and wider-platform acceptance remain separate.',
            'owner_decision':
                'Source/dependency decision pending for blocked rows; ordinary checked development authorized for remaining rows.',
            'evidence_references':
                reviews,
        }
        tasks.append(task)
    flattened = [
        identifier for task in tasks for identifier in task['upstream_rows']
    ]
    validate_coverage.require(
        len(flattened) == len(set(flattened)) == len(required),
        'Backlog does not account for every required row exactly once')
    return {
        'schema_version':
            1,
        'required_denominator':
            len(required),
        'scope':
            'Finite source/mode backlog, not implementation or verification evidence. P09 owns auxiliary/compatibility classification where no narrower programme family applies.',
        'ready_priority': [
            'P09.mixed_general_solve', 'P09.mixed_positive_solve',
            'P04.partial_mode_evidence', 'P09.dmd',
            'P05.positive_tridiagonal_expert', 'P06.remaining_factors',
            'P07.remaining_spectral', 'P08.remaining_eigen_matrix_equations',
            'P09.remaining_auxiliaries', 'P11.provider_platform_admission'
        ],
        'tasks':
            tasks
    }


def main():
    """Regenerate or check the one maintained inventory-backed backlog."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inventory', required=True, type=pathlib.Path)
    parser.add_argument('--mapping', required=True, type=pathlib.Path)
    parser.add_argument('--dependency-closure',
                        required=True,
                        type=pathlib.Path)
    parser.add_argument('--output', required=True, type=pathlib.Path)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    inputs = [args.inventory, args.mapping, args.dependency_closure]
    result = generate(*(validate_coverage.read_json(path) for path in inputs))
    result['input_sha256'] = {
        path.name: hashlib.sha256(path.read_bytes()).hexdigest()
        for path in inputs
    }
    content = json.dumps(result, indent=2) + '\n'
    if args.check:
        validate_coverage.require(
            args.output.read_text() == content,
            'Remaining backlog differs from source records')
    else:
        args.output.write_text(content)
    print(
        json.dumps({
            'required_rows': result['required_denominator'],
            'family_tasks': len(result['tasks'])
        }))


if __name__ == '__main__':
    main()
