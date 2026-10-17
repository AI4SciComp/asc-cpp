# Copyright 2026 AI4SciComp contributors
# SPDX-License-Identifier: Apache-2.0
"""Independent edge fixtures for the extra-precision decision map."""

import pathlib
import subprocess
import sys
import unittest

import map_extra_precision_dependencies as dependencies


class DependencyClosureTest(unittest.TestCase):
    """Prevent source-only helpers and cycles from becoming false XBLAS blocks."""

    def test_cycle_and_shared_helper_keep_boundaries_distinct(self):
        """One source cycle reaches one external helper through two paths."""
        edges = {
            'driver': {'refine', 'norm'},
            'refine': {'driver', 'extra', 'blas_dgemv_x'},
            'extra': {'blas_dgemv_x'},
            'scale': {'norm'}
        }
        self.assertEqual(
            dependencies.closure(edges, 'driver'),
            ({'driver', 'refine', 'extra'}, {'blas_dgemv_x'}, {'norm'}))
        self.assertEqual(dependencies.closure(edges, 'scale'),
                         ({'scale'}, set(), {'norm'}))

    def test_continuations_declarations_and_comments(self):
        """Function declarations count conservatively; comments do not add calls."""
        source = """*     CALL FALSE_NAME( X )
      EXTERNAL NORM,
     $         REFINE
      CALL REFINE( X,
     $             Y )
      IF ( ACTIVE ) CALL BLAS_DGEMV_X( X )
"""
        edges, calls = dependencies.declared_dependencies(source)
        self.assertEqual(edges, {'norm', 'refine', 'blas_dgemv_x'})
        self.assertEqual(calls, [{
            'callee': 'refine',
            'line': 4
        }, {
            'callee': 'blas_dgemv_x',
            'line': 6
        }])

    def test_validation_survives_optimized_python(self):
        """Stale evidence cannot pass because Python was started with -O."""
        directory = pathlib.Path(dependencies.__file__).parent
        result = subprocess.run([
            sys.executable, '-B', '-O', '-c',
            'from map_extra_precision_dependencies import require; require(False, "stale")'
        ],
                                cwd=directory,
                                capture_output=True,
                                text=True,
                                check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('ValueError: stale', result.stderr)

    def test_cli_requires_bound_inputs(self):
        """A missing source/audit/archive cannot produce a plausible map."""
        script = pathlib.Path(dependencies.__file__)
        result = subprocess.run(
            [sys.executable, '-B', str(script), '--help'],
            capture_output=True,
            text=True,
            check=False)
        self.assertEqual(result.returncode, 0)
        self.assertIn('--check', result.stdout)
        missing = subprocess.run(
            [sys.executable, '-B', str(script)],
            capture_output=True,
            text=True,
            check=False)
        self.assertNotEqual(missing.returncode, 0)


if __name__ == '__main__':
    unittest.main()
