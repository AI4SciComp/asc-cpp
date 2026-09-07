cmake_minimum_required(VERSION 3.25)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(ASCCppDenseMatrixMarketExample LANGUAGES CXX)
  include(CTest)
  find_package(ASCCpp 0.9.0 EXACT CONFIG REQUIRED COMPONENTS dense)
  foreach(_unrelated IN ITEMS sparse random utilities dense_lapack
      core_cuda dense_cuda sparse_cuda random_cuda cpp_cuda)
    if(TARGET "ASC::${_unrelated}")
      message(FATAL_ERROR "Dense Matrix Market consumer imported ${_unrelated}.")
    endif()
  endforeach()
  get_property(_languages GLOBAL PROPERTY ENABLED_LANGUAGES)
  if(NOT _languages STREQUAL "CXX")
    message(FATAL_ERROR "Dense Matrix Market consumer enabled ${_languages}.")
  endif()
endif()

add_executable(asc_cpp_example_dense_matrix_market main.cc)
target_compile_features(asc_cpp_example_dense_matrix_market PRIVATE cxx_std_20)
set_target_properties(asc_cpp_example_dense_matrix_market PROPERTIES
  CXX_EXTENSIONS OFF)
target_link_libraries(asc_cpp_example_dense_matrix_market PRIVATE ASC::dense)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/interchange-output")
add_test(NAME asc_cpp.example.dense_matrix_market
  COMMAND asc_cpp_example_dense_matrix_market
    --truncate "${CMAKE_CURRENT_BINARY_DIR}/interchange-output")
if(WIN32)
  set_tests_properties(asc_cpp.example.dense_matrix_market PROPERTIES
    ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:ASC::dense>")
endif()
