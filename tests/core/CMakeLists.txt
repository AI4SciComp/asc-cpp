function(_asc_core_configure_runtime_test target)
  cmake_parse_arguments(TEST "NO_LEAK_SANITIZER" "" "" ${ARGN})
  target_link_libraries("${target}" PRIVATE ASC::core)
  target_include_directories("${target}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  asc_target_enable_cxx20(TARGET "${target}")
  if(ASC_CPP_WARNINGS_AS_ERRORS)
    asc_target_enable_warnings(TARGET "${target}" AS_ERRORS)
  else()
    asc_target_enable_warnings(TARGET "${target}")
  endif()
  set(_sanitizer_arguments ${ASC_CPP_SANITIZER_ARGUMENTS})
  if(TEST_NO_LEAK_SANITIZER)
    # This subprocess replaces global new/delete to prove FatalContract does
    # not allocate. LeakSanitizer also owns those symbols and cannot
    # instrument an intentionally aborting process without defeating the
    # contract being tested.
    list(REMOVE_ITEM _sanitizer_arguments LEAK)
  endif()
  if(_sanitizer_arguments)
    asc_target_enable_sanitizers(
      TARGET "${target}"
      ${_sanitizer_arguments}
    )
  endif()
  unset(_sanitizer_arguments)
endfunction()

set(_asc_core_runtime_test_sources
  configuration_test.cc
  io_test.cc
  memory_execution_test.cc
  status_result_test.cc
  types_extents_test.cc
)

set(_asc_core_runtime_working_directory
  "${CMAKE_CURRENT_BINARY_DIR}/runtime working directory with spaces"
)
file(MAKE_DIRECTORY "${_asc_core_runtime_working_directory}")

foreach(_source IN LISTS _asc_core_runtime_test_sources)
  get_filename_component(_name "${_source}" NAME_WE)
  set(_target "asc_core_${_name}")
  add_executable("${_target}" "${_source}")
  _asc_core_configure_runtime_test("${_target}")
  asc_register_test(
    NAME "asc_cpp.core.${_name}"
    TARGET "${_target}"
    LABELS "core;runtime"
    WORKING_DIRECTORY "${_asc_core_runtime_working_directory}"
  )
endforeach()

add_executable(asc_core_matrix_market_helpers_test
  "${PROJECT_SOURCE_DIR}/tests/sparse/matrix_market_helpers_test.cc"
  "${PROJECT_SOURCE_DIR}/tests/sparse/allocation_probe.cc")
_asc_core_configure_runtime_test(asc_core_matrix_market_helpers_test)
target_include_directories(asc_core_matrix_market_helpers_test PRIVATE
  "${PROJECT_SOURCE_DIR}/tests" "${PROJECT_SOURCE_DIR}/tests/sparse")
asc_register_test(NAME asc_cpp.core.matrix_market_helpers_test
  TARGET asc_core_matrix_market_helpers_test
  LABELS core runtime matrix-market allocation)

add_executable(asc_core_array_parse_test array_parse_test.cc
  "${PROJECT_SOURCE_DIR}/tests/sparse/allocation_probe.cc")
_asc_core_configure_runtime_test(asc_core_array_parse_test)
target_include_directories(asc_core_array_parse_test PRIVATE
  "${PROJECT_SOURCE_DIR}/tests" "${PROJECT_SOURCE_DIR}/tests/sparse")
asc_register_test(NAME asc_cpp.core.array_parse_test
  TARGET asc_core_array_parse_test
  LABELS core runtime array-io allocation)

add_executable(asc_core_contract_failure_test contract_failure_test.cc)
_asc_core_configure_runtime_test(asc_core_contract_failure_test)

add_executable(
  asc_core_result_fatal_test
  result_fatal_no_allocation_test.cc
)
_asc_core_configure_runtime_test(
  asc_core_result_fatal_test
  NO_LEAK_SANITIZER
)

foreach(_mode IN ITEMS check result dcheck file-move-assignment)
  add_test(
    NAME "asc_cpp.core.contract_failure.${_mode}"
    COMMAND
      "${CMAKE_COMMAND}"
      "-DEXECUTABLE:FILEPATH=$<TARGET_FILE:asc_core_contract_failure_test>"
      "-DMODE:STRING=${_mode}"
      "-DWORK_DIR:PATH=${_asc_core_runtime_working_directory}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_contract_failure.cmake"
  )
endforeach()

add_test(
  NAME asc_cpp.core.result_fatal_no_allocation
  COMMAND
    "${CMAKE_COMMAND}"
    "-DEXECUTABLE:FILEPATH=$<TARGET_FILE:asc_core_result_fatal_test>"
    "-DMODE:STRING=result-no-allocation"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_contract_failure.cmake"
)

set_tests_properties(
  asc_cpp.core.contract_failure.check
  asc_cpp.core.contract_failure.result
  asc_cpp.core.contract_failure.dcheck
  asc_cpp.core.contract_failure.file-move-assignment
  asc_cpp.core.result_fatal_no_allocation
  PROPERTIES
    LABELS "contracts;core;runtime"
    TIMEOUT 30
)

unset(_asc_core_runtime_test_sources)
unset(_asc_core_runtime_working_directory)
