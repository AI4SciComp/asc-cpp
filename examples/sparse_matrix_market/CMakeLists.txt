cmake_minimum_required(VERSION 3.25)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(ASCCppSparseMatrixMarketExample LANGUAGES CXX)
  include(CTest)
  # Windows may enable RC while selecting its C++ toolchain. Package lookup
  # must preserve the languages that the standalone project already enabled.
  get_property(_languages_before GLOBAL PROPERTY ENABLED_LANGUAGES)
  find_package(ASCCpp 0.9.0 EXACT CONFIG REQUIRED COMPONENTS sparse)
  foreach(_unrelated IN ITEMS dense random utilities dense_lapack
      core_cuda dense_cuda sparse_cuda random_cuda cpp_cuda)
    if(TARGET "ASC::${_unrelated}")
      message(FATAL_ERROR "Sparse Matrix Market consumer imported ${_unrelated}.")
    endif()
  endforeach()
  get_property(_languages GLOBAL PROPERTY ENABLED_LANGUAGES)
  if(NOT _languages STREQUAL _languages_before)
    message(FATAL_ERROR "Sparse Matrix Market consumer enabled ${_languages}.")
  endif()
endif()

add_executable(asc_cpp_example_sparse_matrix_market main.cc)
target_compile_features(asc_cpp_example_sparse_matrix_market PRIVATE cxx_std_20)
set_target_properties(asc_cpp_example_sparse_matrix_market PROPERTIES
  CXX_EXTENSIONS OFF)
target_link_libraries(asc_cpp_example_sparse_matrix_market PRIVATE ASC::sparse)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/matrix-market-output")
add_test(
  NAME asc_cpp.example.sparse_matrix_market
  COMMAND asc_cpp_example_sparse_matrix_market
    --truncate "${CMAKE_CURRENT_BINARY_DIR}/matrix-market-output"
)
if(WIN32)
  set_tests_properties(asc_cpp.example.sparse_matrix_market PROPERTIES
    ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:ASC::sparse>"
  )
endif()
