function(_asc_core_configure_runtime_test target)
  target_link_libraries("${target}" PRIVATE ASC::core)
  target_include_directories("${target}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  asc_target_enable_cxx20(TARGET "${target}")
  if(ASC_CPP_WARNINGS_AS_ERRORS)
    asc_target_enable_warnings(TARGET "${target}" AS_ERRORS)
  else()
    asc_target_enable_warnings(TARGET "${target}")
  endif()
  if(ASC_CPP_SANITIZER_ARGUMENTS)
    asc_target_enable_sanitizers(
      TARGET "${target}"
      ${ASC_CPP_SANITIZER_ARGUMENTS}
    )
  endif()
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
    LABELS "core;milestone-1;runtime"
    WORKING_DIRECTORY "${_asc_core_runtime_working_directory}"
  )
endforeach()

add_executable(asc_core_contract_failure_test contract_failure_test.cc)
_asc_core_configure_runtime_test(asc_core_contract_failure_test)

add_executable(
  asc_core_result_fatal_no_allocation_test
  result_fatal_no_allocation_test.cc
)
_asc_core_configure_runtime_test(
  asc_core_result_fatal_no_allocation_test
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
    "-DEXECUTABLE:FILEPATH=$<TARGET_FILE:asc_core_result_fatal_no_allocation_test>"
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
    LABELS "contracts;core;milestone-1;runtime"
    TIMEOUT 30
)

unset(_asc_core_runtime_test_sources)
unset(_asc_core_runtime_working_directory)
