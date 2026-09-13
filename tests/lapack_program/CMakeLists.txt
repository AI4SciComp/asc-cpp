# Development-only tooling tests. Ordinary library/package consumers have no
# Python interpreter dependency and never acquire upstream source here.
find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)

foreach(_suite IN ITEMS lapack lapack_io array_io)
  add_test(
    NAME "asc_cpp.program.${_suite}_tools"
    COMMAND
      "${Python3_EXECUTABLE}" -B -m unittest discover
      -s "${PROJECT_SOURCE_DIR}/tools/${_suite}"
      -p "*test*.py" -v
  )
  set_tests_properties(
    "asc_cpp.program.${_suite}_tools"
    PROPERTIES
      LABELS "program;contracts;lapack-array-io"
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  )
endforeach()

add_test(
  NAME asc_cpp.program.coverage_integrity
  COMMAND
    "${Python3_EXECUTABLE}" -B
    "${PROJECT_SOURCE_DIR}/tools/lapack/validate_coverage.py"
    --inventory "${PROJECT_SOURCE_DIR}/docs/contracts/lapack-upstream-inventory.json"
    --mapping "${PROJECT_SOURCE_DIR}/docs/contracts/lapack-coverage.yaml"
    --provider-lock "${PROJECT_SOURCE_DIR}/docs/contracts/lapack-provider-lock.json"
    --evidence "${PROJECT_SOURCE_DIR}/docs/contracts/lapack-evidence.json"
    --source-root "${PROJECT_SOURCE_DIR}"
    --evidence-root "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(
  asc_cpp.program.coverage_integrity
  PROPERTIES LABELS "program;contracts;lapack-array-io"
)
unset(_suite)
