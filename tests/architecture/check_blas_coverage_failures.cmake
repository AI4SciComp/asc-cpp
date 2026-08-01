cmake_minimum_required(VERSION 3.25)

foreach(_required_variable IN ITEMS SOURCE_DIR TEST_BINARY_DIR)
  if(NOT DEFINED "${_required_variable}"
     OR "${${_required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${_required_variable} is required.")
  endif()
endforeach()

set(_manifest_path
  "${SOURCE_DIR}/docs/development/asc-cpp-architecture/blas-coverage.yaml"
)
set(_expected_report "${SOURCE_DIR}/docs/blas-coverage.md")
set(_validator "${SOURCE_DIR}/cmake/GenerateBlasCoverage.cmake")
foreach(_required_path IN ITEMS
    "${_manifest_path}"
    "${_expected_report}"
  "${_validator}"
)
  if(NOT EXISTS "${_required_path}")
    message(FATAL_ERROR
      "Required BLAS coverage path is absent: ${_required_path}"
    )
  endif()
endforeach()

set(_fixture_directory "${TEST_BINARY_DIR}/blas-coverage-negative")
file(MAKE_DIRECTORY "${_fixture_directory}")
file(READ "${_manifest_path}" _valid_manifest)

function(
  _expect_validation_failure
  name
  manifest_variable
  expected_fragment
  compare_report
)
  set(_fixture "${_fixture_directory}/${name}.yaml")
  set(_output "${_fixture_directory}/${name}.md")
  file(WRITE "${_fixture}" "${${manifest_variable}}")

  set(_command
    "${CMAKE_COMMAND}"
    "-DSOURCE_DIR:PATH=${SOURCE_DIR}"
    "-DBLAS_MANIFEST:FILEPATH=${_fixture}"
    "-DOUTPUT_REPORT:FILEPATH=${_output}"
  )
  if(compare_report)
    list(APPEND _command "-DEXPECTED_REPORT:FILEPATH=${_expected_report}")
  endif()
  list(APPEND _command -P "${_validator}")

  execute_process(
    COMMAND ${_command}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
  )
  if(_result EQUAL 0)
    message(FATAL_ERROR
      "BLAS coverage negative fixture '${name}' unexpectedly passed."
    )
  endif()
  set(_log "${_stdout}\n${_stderr}")
  string(FIND "${_log}" "${expected_fragment}" _fragment_position)
  if(_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "BLAS coverage negative fixture '${name}' failed for the wrong reason.\n"
      "Expected fragment: ${expected_fragment}\n"
      "Validator output:\n${_log}"
    )
  endif()
endfunction()

string(REPLACE
  "expected_dense_rows: 150"
  "expected_dense_rows: 149"
  _self_declared_count
  "${_valid_manifest}"
)
_expect_validation_failure(
  self-declared-count
  _self_declared_count
  "the Issue 5 contract freezes '150'"
  FALSE
)

string(REGEX REPLACE
  "(^|\n)    implementation: \"[^\"]+\""
  "\\1    implementation: \"\""
  _missing_implementation
  "${_valid_manifest}"
)
_expect_validation_failure(
  missing-implementation
  _missing_implementation
  "implementation requires a repository path"
  FALSE
)

string(REPLACE
  "backend_cuda: \"verified\""
  "backend_cuda: \"planned\""
  _partially_verified
  "${_valid_manifest}"
)
_expect_validation_failure(
  partially-verified
  _partially_verified
  "verified backends out of 2 applicable backends"
  FALSE
)

string(REPLACE
  "public_api: \"asc::Rotg\""
  "public_api: \"asc::MissingRotg\""
  _missing_public_api
  "${_valid_manifest}"
)
_expect_validation_failure(
  missing-public-api
  _missing_public_api
  "public API 'asc::MissingRotg' is absent"
  FALSE
)

string(REPLACE
  "implementation: \"src/dense/blas_level1.cc\""
  "implementation: \"src/dense/blas_level2.cc\""
  _changed_evidence
  "${_valid_manifest}"
)
_expect_validation_failure(
  changed-evidence
  _changed_evidence
  "BLAS coverage evidence differs from its frozen Issue 11 identity"
  FALSE
)

string(REPLACE
  "sparse_analogue: \"usga,usgz,ussc\"\n    status: \"verified\""
  "sparse_analogue: \"usga,usgz,ussc\"\n    status: \"planned\""
  _incomplete_crosswalk
  "${_valid_manifest}"
)
_expect_validation_failure(
  incomplete-crosswalk
  _incomplete_crosswalk
  "Dense-to-sparse 'copy' is incomplete: planned"
  FALSE
)

string(REGEX REPLACE
  "evidence_date: \"[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]\""
  "evidence_date: \"2099-01-01\""
  _stale_report
  "${_valid_manifest}"
)
_expect_validation_failure(
  stale-report
  _stale_report
  "Generated BLAS report differs"
  TRUE
)

message(STATUS "BLAS coverage negative validation fixtures passed.")
