cmake_minimum_required(VERSION 3.25)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(ASCCppSparseArchiveExample LANGUAGES CXX)
  include(CTest)
  find_package(ASCCpp 0.9.0 EXACT CONFIG REQUIRED COMPONENTS sparse)
  foreach(_unrelated IN ITEMS dense random utilities dense_lapack core_cuda)
    if(TARGET "ASC::${_unrelated}")
      message(FATAL_ERROR "Sparse archive consumer imported ${_unrelated}.")
    endif()
  endforeach()
endif()

add_executable(asc_cpp_example_sparse_array_io main.cc)
target_compile_features(asc_cpp_example_sparse_array_io PRIVATE cxx_std_20)
set_target_properties(asc_cpp_example_sparse_array_io PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(asc_cpp_example_sparse_array_io PRIVATE ASC::sparse)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/archive-output")
add_test(
  NAME asc_cpp.example.sparse_array_io
  COMMAND asc_cpp_example_sparse_array_io
    --truncate "${CMAKE_CURRENT_BINARY_DIR}/archive-output"
)
if(WIN32)
  set_tests_properties(asc_cpp.example.sparse_array_io PROPERTIES
    ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:ASC::sparse>"
  )
endif()
