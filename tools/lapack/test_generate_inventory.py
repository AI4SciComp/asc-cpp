#!/usr/bin/env python3
"""Independent, small Fortran fixtures for the offline inventory reader."""

import importlib.util
import pathlib
import subprocess
import sys
import unittest
from unittest import mock


_SCRIPT = pathlib.Path(__file__).with_name("generate_inventory.py")
_SPEC = importlib.util.spec_from_file_location("asc_lapack_inventory", _SCRIPT)
inventory = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = inventory
_SPEC.loader.exec_module(inventory)


class FortranReaderTest(unittest.TestCase):
    """Check declarations independently of the pinned upstream corpus."""

    def test_fixed_continuation_and_inline_comment(self):
        source = """* SUBROUTINE PHANTOM(X)
      SUBROUTINE REAL_ENTRY(N, A,
     $                      INFO) ! trailing comment
      INTEGER N, INFO
      DOUBLE PRECISION A(N,*)
      END
"""
        routines, units = inventory.parse_fortran(source, "SRC/fixture.f")
        self.assertEqual(units, [])
        self.assertEqual([row["routine"] for row in routines], ["real_entry"])
        self.assertEqual([arg["name"] for arg in routines[0]["arguments"]],
                         ["n", "a", "info"])
        self.assertEqual(routines[0]["arguments"][1]["type"], "f64")
        self.assertEqual(routines[0]["arguments"][1]["dimensions"], "(N,*)")

    def test_multiple_functions_are_not_filename_guesses(self):
        source = """      REAL FUNCTION FIRST(X)
      REAL X
      FIRST = X
      END
      DOUBLE PRECISION FUNCTION SECOND(Y)
      DOUBLE PRECISION Y
      SECOND = Y
      END
"""
        routines, _ = inventory.parse_fortran(source, "SRC/unrelated.f")
        self.assertEqual([row["routine"] for row in routines], ["first", "second"])
        self.assertEqual([row["return_type"]["type"] for row in routines],
                         ["f32", "f64"])
        self.assertEqual([row["scope"] for row in routines], [[], []])

    def test_free_form_kind_aliases_and_continuation(self):
        source = """!> \\param[in] X
!> \\param[out] Y
subroutine rotation(x, &
 & y)
  use la_constants, only: wp => dp
  real(wp) :: x
  complex(wp), intent(out) :: y
end subroutine rotation
"""
        routines, _ = inventory.parse_fortran(source, "SRC/rotation.f90")
        args = routines[0]["arguments"]
        self.assertEqual([arg["type"] for arg in args], ["f64", "c128"])
        self.assertEqual([arg["intent"] for arg in args], ["in", "out"])

    def test_mixed_precision_is_derived_from_actual_arguments(self):
        source = """      SUBROUTINE NAMED(A, B, R)
      COMPLEX*16 A(*)
      COMPLEX*8 B(*)
      DOUBLE PRECISION R(*)
      END
"""
        routines, _ = inventory.parse_fortran(source, "SRC/named.f")
        self.assertEqual([arg["type"] for arg in routines[0]["arguments"]],
                         ["c128", "c64", "f64"])

    def test_iso_fortran_env_parameter_selects_actual_precision(self):
        source = """subroutine data_modes(x)
use, intrinsic :: iso_fortran_env, only: real64
integer, parameter :: wp = real64
complex(kind=wp), intent(inout) :: x(*)
end subroutine data_modes
"""
        routines, _ = inventory.parse_fortran(source, "SRC/data_modes.f90")
        self.assertEqual(routines[0]["arguments"][0]["type"], "c128")

    def test_module_and_internal_procedures_remain_distinct(self):
        source = """module numerics
contains
logical function predicate(x)
  real :: x
contains
logical function helper(y)
  real :: y
end function helper
end function predicate
logical function next(x)
  real :: x
end function next
end module numerics
"""
        routines, units = inventory.parse_fortran(source, "SRC/module.f90")
        self.assertEqual([row["scope"] for row in routines],
                         [["numerics"], ["numerics", "predicate"], ["numerics"]])
        self.assertEqual(units[0]["kind"], "module")

    def test_preprocessor_alternatives_are_retained(self):
        source = """#ifdef EXTENDED
      SUBROUTINE EXTENDED(A)
      REAL A
      END
#else
      SUBROUTINE ORDINARY(A)
      REAL A
      END
#endif
"""
        routines, _ = inventory.parse_fortran(source, "SRC/conditional.F")
        self.assertEqual(len(routines), 2)
        self.assertEqual(routines[0]["preprocessor_conditions"], ["ifdef EXTENDED"])
        self.assertEqual(routines[1]["preprocessor_conditions"],
                         ["ifdef EXTENDED -> else"])

    def test_callback_is_not_falsely_inferred_as_real(self):
        source = """      SUBROUTINE DRIVER(SELECT, APPLY)
      LOGICAL SELECT
      EXTERNAL SELECT, APPLY
      END
"""
        routines, _ = inventory.parse_fortran(source, "SRC/driver.f")
        args = routines[0]["arguments"]
        self.assertEqual([arg["external_procedure"] for arg in args], [True, True])
        self.assertEqual([arg["type"] for arg in args],
                         ["fortran_logical", "external_procedure"])

    def test_quotes_and_parentheses_do_not_split_arguments(self):
        self.assertEqual(inventory.split_arguments("A(N,*), B, 'comma,!''x'"),
                         ["A(N,*)", "B", "'comma,!''x'"])
        statements = inventory.logical_statements("      PRINT *, 'a!b' ! note\n", False)
        self.assertEqual(statements[0].text, "PRINT *, 'a!b'")


class DiscoveryTest(unittest.TestCase):
    """Independent discovery evidence must not depend on LAPACKE availability."""

    def test_cmake_conditions_and_make_sources(self):
        references = inventory.parse_build_references(
            "if(USE_XBLAS)\nset(XSRC extra.f)\nendif()\n",
            "SRC/CMakeLists.txt", {"SRC/extra.f"})
        self.assertEqual(references[0]["conditions"], ["USE_XBLAS"])
        self.assertEqual(references[0]["source_paths"], ["SRC/extra.f"])
        references = inventory.parse_build_references(
            "AUX = ordinary.o\n", "SRC/Makefile", {"SRC/ordinary.f90"})
        self.assertEqual(references[0]["source_paths"], ["SRC/ordinary.f90"])

    def test_absent_documentation_placeholder_is_not_a_routine(self):
        groups = inventory.parse_documentation_groups(
            "@defgroup parent Parent\n@{\n"
            "@defgroup absent absent: [not available]\n@}\n")
        self.assertEqual(groups[1]["parents"], ["parent"])
        self.assertTrue(groups[1]["absent_placeholder"])

    def test_expert_auxiliary_is_required_despite_missing_wrapper(self):
        row = {"path": "SRC/example.f", "routine": "example", "scope": [],
               "documentation": {"upstream_categories": ["auxiliary"], "groups": []}}
        category, required, _ = inventory._classify(row)
        self.assertEqual(category, "expert_auxiliary")
        self.assertTrue(required)
        row["path"] = "SRC/DEPRECATED/example.f"
        category, required, _ = inventory._classify(row)
        self.assertEqual(category, "deprecated_compatibility")
        self.assertTrue(required)

    def test_unknown_source_classification_stays_unresolved(self):
        row = {"path": "SRC/unknown.f", "routine": "unknown", "scope": [],
               "documentation": {"upstream_categories": [], "groups": []}}
        self.assertEqual(inventory._classify(row)[0], "unresolved")

    def test_wrong_pin_rejected_before_git(self):
        with mock.patch.object(inventory, "_git") as git:
            with self.assertRaises(ValueError):
                inventory.verify_source(pathlib.Path("unused"), "0" * 40)
        git.assert_not_called()

    def test_dirty_upstream_tree_rejected(self):
        with mock.patch.object(inventory, "_git",
                               side_effect=[inventory.PINNED_COMMIT, " M SRC/a.f"]):
            with self.assertRaisesRegex(ValueError, "pristine"):
                inventory.verify_source(pathlib.Path("unused"), inventory.PINNED_COMMIT)

    def test_content_integrity_ignores_git_index_assumptions(self):
        inventory.verify_blob(b"", "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391", "empty")
        with self.assertRaisesRegex(ValueError, "pinned Git blob"):
            inventory.verify_blob(b"modified", "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391", "empty")

    def test_help_and_failure_exits(self):
        result = subprocess.run([sys.executable, str(_SCRIPT), "--help"],
                                capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0)
        self.assertIn("--check", result.stdout)
        result = subprocess.run([sys.executable, str(_SCRIPT)],
                                capture_output=True, text=True, check=False)
        self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
