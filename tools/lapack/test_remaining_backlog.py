# Copyright 2026 AI4SciComp contributors
# SPDX-License-Identifier: Apache-2.0
"""Check that scheduling records preserve actual requirements and partial rows."""

import unittest

import generate_remaining_backlog as backlog
import validate_coverage


class RemainingBacklogTest(unittest.TestCase):
    """A partial registration and a source block are independent dimensions."""

    @staticmethod
    def fixture():
        """Three required rows and one excluded row with explicit source facts."""
        rows = []
        mapping = []
        for name, family, required, state in (('dsgesv', 'gesv_mixed', True,
                                               'in_progress'),
                                              ('zcgesv', 'gesv_mixed', True,
                                               'not_started'),
                                              ('dgerfsx', 'gerfsx', True,
                                               'not_started'), ('tester',
                                                                'test', False,
                                                                'not_started')):
            rows.append({
                'id':
                    'lapack.' + name,
                'routine':
                    name,
                'required_profiles': ['reference_cpu_full'] if required else [],
                'family': [family],
                'source_instances': [{
                    'path': 'SRC/' + name + '.f',
                    'required': required
                }]
            })
            mapping.append({
                'id': 'lapack.' + name,
                'implementations': {
                    'reference_cpu': {
                        'state': state
                    }
                },
                'contract': {
                    'mode_cases': []
                },
                'implementation_artifacts': []
            })
        return {
            'routines': rows
        }, {
            'routines': mapping
        }, {
            'rows': [{
                'id': 'lapack.dgerfsx'
            }]
        }

    def test_exact_denominator_and_distinct_routes(self):
        """Neither mixed declarations nor excluded tests increase the count."""
        result = backlog.generate(*self.fixture())
        self.assertEqual(result['required_denominator'], 3)
        self.assertEqual(len(result['tasks']), 2)
        mixed = next(task for task in result['tasks']
                     if task['task_id'].endswith('gesv_mixed'))
        self.assertEqual(mixed['upstream_rows'],
                         ['lapack.dsgesv', 'lapack.zcgesv'])
        self.assertEqual(mixed['blocked_rows'], [])
        self.assertEqual(mixed['route_states']['lapack.zcgesv'], 'not_started')
        extra = next(task for task in result['tasks'] if task['blocked_rows'])
        self.assertEqual(extra['blocked_rows'], ['lapack.dgerfsx'])

    def test_missing_and_duplicate_rows_fail(self):
        """A plausible smaller list cannot masquerade as exhaustive closure."""
        inventory, mapping, blocked = self.fixture()
        mapping['routines'].pop(0)
        with self.assertRaises(validate_coverage.ValidationError):
            backlog.generate(inventory, mapping, blocked)
        inventory, mapping, blocked = self.fixture()
        inventory['routines'].append(inventory['routines'][0])
        with self.assertRaises(validate_coverage.ValidationError):
            backlog.generate(inventory, mapping, blocked)

    def test_programme_ownership(self):
        """SVD, eigenproblems and mixed drivers retain separate Pxx owners."""
        for family, package in [('getrf', 'P04'), ('pttrs', 'P05'),
                                ('geqrf', 'P06'), ('gesvd', 'P07'),
                                ('heev', 'P08'), ('trsyl', 'P08'),
                                ('posv_mixed', 'P09'), ('la_wwaddw', 'P09')]:
            with self.subTest(family=family):
                self.assertEqual(backlog.owner(family), package)


if __name__ == '__main__':
    unittest.main()
