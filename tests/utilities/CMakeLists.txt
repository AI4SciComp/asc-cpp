function(asc_cpp_add_utilities_test name source)
  add_executable("asc_cpp_test_utilities_${name}" "${source}"
                 "${PROJECT_SOURCE_DIR}/tests/unit_test_main.cc")
  target_link_libraries(
    "asc_cpp_test_utilities_${name}"
    PRIVATE ASC::utilities GTest::gtest asc_cpp_project_options
  )
  asc_register_test(
    NAME "asc-cpp.utilities.${name}"
    TARGET "asc_cpp_test_utilities_${name}"
    LABELS "unit;utilities"
    TIMEOUT 60
  )
endfunction()

asc_cpp_add_utilities_test(config unit_test_config.cc)
asc_cpp_add_utilities_test(cli unit_test_cli.cc)
asc_cpp_add_utilities_test(timer unit_test_timer.cc)

if(ASC_CPP_ENABLE_EXCEPTIONS)
  add_executable(asc_cpp_test_utilities_release_contract
                 release_contract_test.cc)
  target_compile_definitions(
    asc_cpp_test_utilities_release_contract PRIVATE NDEBUG
  )
  target_link_libraries(
    asc_cpp_test_utilities_release_contract
    PRIVATE ASC::utilities asc_cpp_project_options
  )
  asc_register_test(
    NAME asc-cpp.utilities.release-contract
    TARGET asc_cpp_test_utilities_release_contract
    LABELS "contract;release;utilities"
    TIMEOUT 30
  )
endif()

add_test(
  NAME asc-cpp.utilities.dependency-boundary
  COMMAND "${CMAKE_COMMAND}"
    "-DASC_CPP_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/dependency_check.cmake"
)
set_property(
  TEST asc-cpp.utilities.dependency-boundary
  PROPERTY LABELS "architecture;compile;utilities"
)
set_property(
  TEST asc-cpp.utilities.dependency-boundary PROPERTY TIMEOUT 30
)

add_executable(
  asc_cpp_test_utilities_odr odr_a.cc odr_b.cc odr_main.cc
)
target_link_libraries(
  asc_cpp_test_utilities_odr PRIVATE ASC::utilities asc_cpp_project_options
)
asc_register_test(
  NAME asc-cpp.utilities.odr
  TARGET asc_cpp_test_utilities_odr
  LABELS "compile;odr;utilities"
  TIMEOUT 30
)

get_target_property(_asc_cpp_utilities_links
                    asc_utilities INTERFACE_LINK_LIBRARIES)
set(_asc_cpp_utilities_component_links ${_asc_cpp_utilities_links})
list(REMOVE_ITEM _asc_cpp_utilities_component_links
     "$<LINK_ONLY:$<BUILD_INTERFACE:asc_cpp_project_options>>")
if(NOT _asc_cpp_utilities_component_links STREQUAL "asc_core" AND
   NOT _asc_cpp_utilities_component_links STREQUAL "ASC::core")
  message(FATAL_ERROR
    "ASC::utilities must link publicly only to ASC::core: "
    "${_asc_cpp_utilities_links}"
  )
endif()
