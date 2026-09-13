# Copyright 2026 AI4SciComp contributors
# SPDX-License-Identifier: Apache-2.0
"""Independent fixture expectations for offline build-list/symbol reconciliation."""

import pathlib
import subprocess
import sys
import unittest
from unittest import mock

import audit_build_graph as audit


class BuildGraphTest(unittest.TestCase):
    """Exercise transitive, conditional, duplicate and malformed source lists."""

    def test_transitive_and_duplicate_sources_keep_distinct_guards(self):
        """A helper can be selected by either real precision, without duplication."""
        fixture = """set(COMMON helper.f)
set(SINGLE single.f ${COMMON})
set(DOUBLE double.f ${COMMON})
if(USE_EXTRA)
list(APPEND DOUBLE extra.f)
endif()
set(SOURCES)
if(BUILD_A)
list(APPEND SOURCES ${SINGLE})
endif()
if(BUILD_B)
list(APPEND SOURCES ${DOUBLE})
endif()
list(REMOVE_DUPLICATES SOURCES)
"""
        graph = audit.normalized_sources(audit.list_graph(
            fixture, "SRC/CMakeLists.txt")["SOURCES"])
        self.assertEqual(graph["SRC/helper.f"]["conditions_dnf"],
                         [["BUILD_A"], ["BUILD_B"]])
        self.assertEqual(graph["SRC/helper.f"]["occurrences_before_deduplication"], 2)
        self.assertEqual(graph["SRC/extra.f"]["conditions_dnf"],
                         [["BUILD_B", "USE_EXTRA"]])
        self.assertEqual(graph["SRC/helper.f"]["expansion_paths"][1],
                         ("SRC/CMakeLists.txt:1:COMMON", "SRC/CMakeLists.txt:3:DOUBLE",
                          "SRC/CMakeLists.txt:12:SOURCES"))

    def test_child_macro_retains_definition_and_call_conditions(self):
        """Directory transfer keeps source locations and downstream guards."""
        children = {"src": audit.list_graph("set(EXTRA special.c)",
                                            "LAPACKE/src/CMakeLists.txt")}
        graph = audit.list_graph("""set(SOURCES)
append_subdir_files(EXTRA "src")
if(USE_EXTRA)
list(APPEND SOURCES ${EXTRA})
endif()
""", "LAPACKE/CMakeLists.txt", children)
        self.assertEqual(graph["SOURCES"][0].path, "LAPACKE/src/special.c")
        self.assertEqual(graph["SOURCES"][0].conditions, ("USE_EXTRA",))
        self.assertEqual(len(graph["SOURCES"][0].history), 3)

    def test_timing_variable_remains_explicit(self):
        """Platform-dependent timing inputs must not disappear silently."""
        result = audit.list_graph("set(SOURCES ${SECOND_SRC})", "SRC/CMakeLists.txt")
        self.assertEqual(result["SOURCES"][0].path, "${SECOND_SRC}")

    def test_comments_quotes_and_multiline_lists(self):
        """Comment delimiters inside quotes are ordinary literal characters."""
        self.assertEqual(audit.commands('set(A "a#b.f"\n # ) comment\n other.f)\n'),
                         [("set", ["A", "a#b.f", "other.f"], 1)])

    def test_unsupported_or_unbalanced_syntax_fails_closed(self):
        """Unknown grammar cannot produce a plausible incomplete graph."""
        for fixture in ("set(A ${MISSING})", "if(A OR B)\nendif()", "endif()",
                        "if(A)", "file(GLOB SOURCES *.f)", "set(A [[literal]])",
                        "set(A unfinished", "if(A)\nset(B file.f)\nendif()"):
            with self.subTest(fixture=fixture), self.assertRaises(ValueError):
                audit.list_graph(fixture, "SRC/CMakeLists.txt")

    def test_boolean_selection_rejects_unknown_options(self):
        """Missing options are errors even if another OR branch selects a file."""
        record = {"conditions_dnf": [["A", "B"], ["C"]]}
        self.assertTrue(audit.selected(record, {"A": "OFF", "B": "OFF", "C": "ON"}))
        self.assertFalse(audit.selected(record, {"A": "ON", "B": "OFF", "C": "OFF"}))
        with self.assertRaises(ValueError):
            audit.selected(record, {"C": "ON"})


class SymbolsTest(unittest.TestCase):
    """Treat symbols and archive members as observations, never routine counts."""

    def test_duplicate_members_and_module_symbols_remain_separate(self):
        """Repeated definitions are not silently collapsed into a capability count."""
        result = audit.parse_symbols("""/some/archive.a[first.f.o]:
solve_ T 0 20
__constants_MOD_value R 0 8
/some/archive.a[second.f.o]:
solve_ T 0 10
""", "lib/archive.a")
        self.assertEqual([row["member"] for row in result["solve_"]],
                         ["first.f.o", "second.f.o"])
        self.assertIn("__constants_MOD_value", result)
        self.assertEqual(result["solve_"][0]["archive"], "lib/archive.a")

    def test_undefined_or_unscoped_records_rejected(self):
        """Malformed records cannot masquerade as positive definition evidence."""
        for fixture in ("solve_ T 0 2", "lib.a[member.o]:\nsolve_ U 0"):
            with self.subTest(fixture=fixture), self.assertRaises(ValueError):
                audit.parse_symbols(fixture, "lib.a")

    def test_help_and_nonzero_missing_arguments(self):
        """The real command exposes usage and rejects incomplete requests."""
        script = pathlib.Path(audit.__file__)
        result = subprocess.run([sys.executable, "-B", str(script), "--help"], check=False,
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("--provider-lock", result.stdout)
        result = subprocess.run([sys.executable, "-B", str(script)], check=False,
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)

    def test_dependency_source_pairs_and_quads(self):
        """Fortran and C object lists use distinct generated record widths."""
        fixture = """set(CMAKE_DEPENDS_CHECK_Fortran "/source/SRC/real.f" "/build/real.o")
set(CMAKE_DEPENDS_DEPENDENCY_FILES "/source/LAPACKE/wrapper.c" "wrapper.o" "gcc" "wrapper.d")
"""
        self.assertEqual(audit.dependency_sources(fixture, pathlib.Path("/source")),
                         ["LAPACKE/wrapper.c", "SRC/real.f"])

    def test_dependency_source_escape_and_partial_record(self):
        """Sources outside the verified root and broken tuple widths are rejected."""
        for fixture in ('set(CMAKE_DEPENDS_CHECK_Fortran "/elsewhere/a.f" "a.o")',
                        'set(CMAKE_DEPENDS_DEPENDENCY_FILES "/source/a.c" "a.o")'):
            with self.subTest(fixture=fixture), self.assertRaises(ValueError):
                audit.dependency_sources(fixture, pathlib.Path("/source"))

    def test_required_source_absence_is_an_error(self):
        """An unknown required source cannot disappear from the denominator."""
        routine = {"id": "lapack.solve", "source_instances": [
            {"path": "SRC/unknown.f", "sha256": "fixture",
             "classification": "public_driver", "required": True}]}
        with self.assertRaisesRegex(ValueError, "no reviewed source selection"):
            audit.routine_sources(routine, {"lapack": {}, "blas": {}},
                                  pathlib.Path("/source"))

    def test_missing_and_unexpected_symbol_are_disagreements(self):
        """Neither a missing object nor an unselected export passes reconciliation."""
        inventory = {"routines": [{"id": "lapack.solve", "routine": "solve",
                                    "required_profiles": ["reference_cpu_full"],
                                    "classification": "public_driver",
                                    "interface_routes": []}]}
        graph = {"lapack": {"SRC/solve.f": {"conditions_dnf": [["BUILD_A"]]}}}
        providers = [{"label": "missing", "options": {"BUILD_A": "ON"}, "symbols": {}},
                     {"label": "unexpected", "options": {"BUILD_A": "OFF"},
                      "symbols": {"solve_": [{"member": "solve.o"}]}}]
        with mock.patch.object(audit, "routine_sources", return_value=(
                [{"path": "SRC/solve.f", "libraries": ["lapack"]}], [])):
            result = audit.reconcile(inventory, graph, providers, pathlib.Path("/source"))
        self.assertEqual(len(result["routines"]), 1)
        self.assertEqual(result["source_symbol_mismatches"], [
            {"id": "lapack.solve", "provider": "missing", "selected": True, "defined": False},
            {"id": "lapack.solve", "provider": "unexpected", "selected": False, "defined": True}])


if __name__ == "__main__":
    unittest.main()
