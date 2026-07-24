function(asc_cpp_add_array_test name source)
  add_executable("asc_cpp_test_array_${name}" "${source}"
                 "${PROJECT_SOURCE_DIR}/tests/unit_test_main.cc")
  target_link_libraries(
    "asc_cpp_test_array_${name}"
    PRIVATE ASC::array GTest::gtest asc_cpp_project_options
  )
  asc_register_test(
    NAME "asc-cpp.array.${name}"
    TARGET "asc_cpp_test_array_${name}"
    LABELS "unit;array"
    TIMEOUT 60
  )
endfunction()

asc_cpp_add_array_test(extents unit_test_extents.cc)
asc_cpp_add_array_test(layout unit_test_layout.cc)
asc_cpp_add_array_test(tensor-view unit_test_tensor_view.cc)
asc_cpp_add_array_test(tensor unit_test_tensor.cc)

if(ASC_CPP_ENABLE_EXCEPTIONS)
  add_executable(asc_cpp_test_array_release_contract
                 release_contract_test.cc)
  target_compile_definitions(
    asc_cpp_test_array_release_contract PRIVATE NDEBUG
  )
  target_link_libraries(
    asc_cpp_test_array_release_contract
    PRIVATE ASC::array asc_cpp_project_options
  )
  asc_register_test(
    NAME asc-cpp.array.release-contract
    TARGET asc_cpp_test_array_release_contract
    LABELS "array;contract;release"
    TIMEOUT 30
  )
endif()

add_test(
  NAME asc-cpp.array.dependency-boundary
  COMMAND "${CMAKE_COMMAND}"
    "-DASC_CPP_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/dependency_check.cmake"
)
set_property(
  TEST asc-cpp.array.dependency-boundary
  PROPERTY LABELS "architecture;array;compile"
)
set_property(TEST asc-cpp.array.dependency-boundary PROPERTY TIMEOUT 30)

add_executable(asc_cpp_test_array_odr odr_a.cc odr_b.cc odr_main.cc)
target_link_libraries(
  asc_cpp_test_array_odr PRIVATE ASC::array asc_cpp_project_options
)
asc_register_test(
  NAME asc-cpp.array.odr
  TARGET asc_cpp_test_array_odr
  LABELS "array;compile;odr"
  TIMEOUT 30
)

get_target_property(_asc_cpp_array_links asc_array INTERFACE_LINK_LIBRARIES)
set(_asc_cpp_array_component_links ${_asc_cpp_array_links})
list(REMOVE_ITEM _asc_cpp_array_component_links
     "$<LINK_ONLY:$<BUILD_INTERFACE:asc_cpp_project_options>>")
if(NOT _asc_cpp_array_component_links STREQUAL "asc_core" AND
   NOT _asc_cpp_array_component_links STREQUAL "ASC::core")
  message(FATAL_ERROR
    "ASC::array must link publicly only to ASC::core: "
    "${_asc_cpp_array_links}"
  )
endif()
