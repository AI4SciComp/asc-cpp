cmake_minimum_required(VERSION 3.25)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(ASCCppDenseFactorizationsExample LANGUAGES CXX)
  include(CTest)
  find_package(ASCCpp 0.9.0 EXACT CONFIG REQUIRED COMPONENTS dense)
  foreach(_unrelated IN ITEMS sparse random utilities dense_lapack
      core_cuda dense_cuda sparse_cuda random_cuda cpp_cuda)
    if(TARGET "ASC::${_unrelated}")
      message(FATAL_ERROR "Native Dense consumer imported ${_unrelated}.")
    endif()
  endforeach()
  get_property(_languages GLOBAL PROPERTY ENABLED_LANGUAGES)
  if(NOT _languages STREQUAL "CXX")
    message(FATAL_ERROR "Native Dense consumer enabled ${_languages}.")
  endif()
endif()

add_executable(asc_cpp_example_dense_factorizations main.cc)
target_compile_features(asc_cpp_example_dense_factorizations PRIVATE cxx_std_20)
set_target_properties(asc_cpp_example_dense_factorizations PROPERTIES
  CXX_EXTENSIONS OFF)
target_link_libraries(asc_cpp_example_dense_factorizations PRIVATE ASC::dense)
add_test(NAME asc_cpp.example.dense_factorizations
  COMMAND asc_cpp_example_dense_factorizations)
if(WIN32)
  set_tests_properties(asc_cpp.example.dense_factorizations PROPERTIES
    ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:ASC::dense>")
endif()
