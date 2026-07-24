function(asc_cpp_add_random_test name source)
  add_executable("asc_cpp_test_random_${name}" "${source}"
                 "${PROJECT_SOURCE_DIR}/tests/unit_test_main.cc")
  target_link_libraries(
    "asc_cpp_test_random_${name}"
    PRIVATE ASC::random GTest::gtest asc_cpp_project_options
  )
  asc_register_test(
    NAME "asc-cpp.random.${name}"
    TARGET "asc_cpp_test_random_${name}"
    LABELS "unit;random"
    TIMEOUT 60
  )
endfunction()

asc_cpp_add_random_test(engine unit_test_engine.cc)
asc_cpp_add_random_test(distribution unit_test_distribution.cc)
asc_cpp_add_random_test(fill unit_test_fill.cc)
target_sources(asc_cpp_test_random_fill PRIVATE allocation_counter.cc)

if(ASC_CPP_ENABLE_EXCEPTIONS)
  add_executable(asc_cpp_test_random_release_contract
                 release_contract_test.cc)
  target_compile_definitions(
    asc_cpp_test_random_release_contract PRIVATE NDEBUG
  )
  target_link_libraries(
    asc_cpp_test_random_release_contract
    PRIVATE ASC::random asc_cpp_project_options
  )
  asc_register_test(
    NAME asc-cpp.random.release-contract
    TARGET asc_cpp_test_random_release_contract
    LABELS "contract;random;release"
    TIMEOUT 30
  )
endif()

add_test(
  NAME asc-cpp.random.dependency-boundary
  COMMAND "${CMAKE_COMMAND}"
    "-DASC_CPP_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/dependency_check.cmake"
)
set_property(
  TEST asc-cpp.random.dependency-boundary
  PROPERTY LABELS "architecture;compile;random"
)
set_property(
  TEST asc-cpp.random.dependency-boundary PROPERTY TIMEOUT 30
)

add_executable(asc_cpp_test_random_odr odr_a.cc odr_b.cc odr_main.cc)
target_link_libraries(
  asc_cpp_test_random_odr PRIVATE ASC::random asc_cpp_project_options
)
asc_register_test(
  NAME asc-cpp.random.odr
  TARGET asc_cpp_test_random_odr
  LABELS "compile;odr;random"
  TIMEOUT 30
)

get_target_property(_asc_cpp_random_links asc_random
                    INTERFACE_LINK_LIBRARIES)
set(_asc_cpp_random_component_links ${_asc_cpp_random_links})
list(REMOVE_ITEM _asc_cpp_random_component_links
     "$<LINK_ONLY:$<BUILD_INTERFACE:asc_cpp_project_options>>")
if(NOT asc_array IN_LIST _asc_cpp_random_component_links OR
   NOT asc_core IN_LIST _asc_cpp_random_component_links)
  message(FATAL_ERROR
    "ASC::random must link publicly to ASC::array and ASC::core: "
    "${_asc_cpp_random_links}"
  )
endif()
list(REMOVE_ITEM _asc_cpp_random_component_links asc_array asc_core)
if(_asc_cpp_random_component_links)
  message(FATAL_ERROR
    "ASC::random has an unapproved public dependency: "
    "${_asc_cpp_random_component_links}"
  )
endif()
