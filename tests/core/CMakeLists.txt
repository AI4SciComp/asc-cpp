function(asc_cpp_add_core_test name source)
  add_executable("asc_cpp_test_core_${name}" "${source}"
                 "${PROJECT_SOURCE_DIR}/tests/unit_test_main.cc")
  target_link_libraries(
    "asc_cpp_test_core_${name}"
    PRIVATE ASC::core GTest::gtest asc_cpp_project_options
  )
  asc_register_test(
    NAME "asc-cpp.core.${name}"
    TARGET "asc_cpp_test_core_${name}"
    LABELS "unit;core"
    TIMEOUT 60
  )
endfunction()

asc_cpp_add_core_test(types unit_test_types.cc)
asc_cpp_add_core_test(base unit_test_base.cc)
asc_cpp_add_core_test(memory-resource unit_test_memory_resource.cc)
asc_cpp_add_core_test(buffer unit_test_buffer.cc)
asc_cpp_add_core_test(event unit_test_event.cc)
asc_cpp_add_core_test(execution-context unit_test_execution_context.cc)

if(ASC_CPP_ENABLE_EXCEPTIONS)
  add_executable(asc_cpp_test_core_release_contract
                 release_contract_test.cc)
  target_compile_definitions(asc_cpp_test_core_release_contract PRIVATE NDEBUG)
  target_link_libraries(
    asc_cpp_test_core_release_contract
    PRIVATE ASC::core asc_cpp_project_options
  )
  asc_register_test(
    NAME asc-cpp.core.release-contract
    TARGET asc_cpp_test_core_release_contract
    LABELS "contract;core;release"
    TIMEOUT 30
  )
endif()

add_test(
  NAME asc-cpp.core.dependency-boundary
  COMMAND "${CMAKE_COMMAND}"
    "-DASC_CPP_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/dependency_check.cmake"
)
set_property(
  TEST asc-cpp.core.dependency-boundary
  PROPERTY LABELS "architecture;compile;core"
)
set_property(TEST asc-cpp.core.dependency-boundary PROPERTY TIMEOUT 30)

add_executable(asc_cpp_test_core_odr odr_a.cc odr_b.cc odr_main.cc)
target_link_libraries(
  asc_cpp_test_core_odr PRIVATE ASC::core asc_cpp_project_options
)
asc_register_test(
  NAME asc-cpp.core.odr
  TARGET asc_cpp_test_core_odr
  LABELS "compile;core;odr"
  TIMEOUT 30
)

get_target_property(_asc_cpp_core_links asc_core INTERFACE_LINK_LIBRARIES)
if(_asc_cpp_core_links MATCHES
   "ASC::(utilities|array|linalg|random|cpp)")
  message(FATAL_ERROR
    "ASC::core has a forbidden higher-component dependency: "
    "${_asc_cpp_core_links}"
  )
endif()
