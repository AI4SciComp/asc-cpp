function(asc_cpp_add_linalg_test name source)
  add_executable("asc_cpp_test_linalg_${name}" "${source}"
                 "${PROJECT_SOURCE_DIR}/tests/unit_test_main.cc")
  target_link_libraries(
    "asc_cpp_test_linalg_${name}"
    PRIVATE ASC::linalg GTest::gtest asc_cpp_project_options
  )
  asc_register_test(
    NAME "asc-cpp.linalg.${name}"
    TARGET "asc_cpp_test_linalg_${name}"
    LABELS "unit;linalg"
    TIMEOUT 60
  )
endfunction()

asc_cpp_add_linalg_test(blas1 unit_test_blas1.cc)
asc_cpp_add_linalg_test(blas2 unit_test_blas2.cc)
asc_cpp_add_linalg_test(blas3 unit_test_blas3.cc)
target_sources(asc_cpp_test_linalg_blas3 PRIVATE allocation_counter.cc)

if(ASC_CPP_ENABLE_EXCEPTIONS)
  add_executable(asc_cpp_test_linalg_release_contract
                 release_contract_test.cc)
  target_compile_definitions(
    asc_cpp_test_linalg_release_contract PRIVATE NDEBUG
  )
  target_link_libraries(
    asc_cpp_test_linalg_release_contract
    PRIVATE ASC::linalg asc_cpp_project_options
  )
  asc_register_test(
    NAME asc-cpp.linalg.release-contract
    TARGET asc_cpp_test_linalg_release_contract
    LABELS "contract;linalg;release"
    TIMEOUT 30
  )
endif()

add_test(
  NAME asc-cpp.linalg.dependency-boundary
  COMMAND "${CMAKE_COMMAND}"
    "-DASC_CPP_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/dependency_check.cmake"
)
set_property(
  TEST asc-cpp.linalg.dependency-boundary
  PROPERTY LABELS "architecture;compile;linalg"
)
set_property(
  TEST asc-cpp.linalg.dependency-boundary PROPERTY TIMEOUT 30
)

add_executable(asc_cpp_test_linalg_odr odr_a.cc odr_b.cc odr_main.cc)
target_link_libraries(
  asc_cpp_test_linalg_odr PRIVATE ASC::linalg asc_cpp_project_options
)
asc_register_test(
  NAME asc-cpp.linalg.odr
  TARGET asc_cpp_test_linalg_odr
  LABELS "compile;linalg;odr"
  TIMEOUT 30
)

get_target_property(_asc_cpp_linalg_links asc_linalg
                    INTERFACE_LINK_LIBRARIES)
set(_asc_cpp_linalg_component_links ${_asc_cpp_linalg_links})
list(REMOVE_ITEM _asc_cpp_linalg_component_links
     "$<LINK_ONLY:$<BUILD_INTERFACE:asc_cpp_project_options>>")
if(NOT asc_array IN_LIST _asc_cpp_linalg_component_links OR
   NOT asc_core IN_LIST _asc_cpp_linalg_component_links)
  message(FATAL_ERROR
    "ASC::linalg must link publicly to ASC::array and ASC::core: "
    "${_asc_cpp_linalg_links}"
  )
endif()
list(REMOVE_ITEM _asc_cpp_linalg_component_links asc_array asc_core)
if(ASC_CPP_ENABLE_EIGEN)
  list(REMOVE_ITEM _asc_cpp_linalg_component_links Eigen3::Eigen)
endif()
if(ASC_CPP_ENABLE_MKL)
  list(REMOVE_ITEM _asc_cpp_linalg_component_links MKL::MKL)
endif()
if(_asc_cpp_linalg_component_links)
  message(FATAL_ERROR
    "ASC::linalg has an unapproved public dependency: "
    "${_asc_cpp_linalg_component_links}"
  )
endif()
