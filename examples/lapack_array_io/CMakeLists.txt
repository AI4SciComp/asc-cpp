cmake_minimum_required(VERSION 3.25)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(ASCCppLapackArrayIoExample LANGUAGES CXX)
  include(CTest)
endif()

option(
  ASC_CPP_EXAMPLES_ENABLE_LAPACK
  "Build the explicitly selected Reference-LAPACK example route"
  OFF
)
set(_asc_array_io_components dense)
if(ASC_CPP_EXAMPLES_ENABLE_LAPACK)
  list(APPEND _asc_array_io_components dense_lapack)
endif()
find_package(
  ASCCpp 0.9.0 EXACT CONFIG REQUIRED
  COMPONENTS ${_asc_array_io_components}
)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  foreach(_unrelated IN ITEMS sparse random utilities core_cuda)
    if(TARGET "ASC::${_unrelated}")
      message(FATAL_ERROR "Dense archive consumer imported ${_unrelated}.")
    endif()
  endforeach()
  if(NOT ASC_CPP_EXAMPLES_ENABLE_LAPACK AND TARGET ASC::dense_lapack)
    message(FATAL_ERROR "Native archive consumer imported the optional provider.")
  endif()
endif()

add_executable(asc_cpp_example_lapack_array_io main.cc)
target_compile_features(asc_cpp_example_lapack_array_io PRIVATE cxx_std_20)
set_target_properties(
  asc_cpp_example_lapack_array_io PROPERTIES CXX_EXTENSIONS OFF
)
target_link_libraries(asc_cpp_example_lapack_array_io PRIVATE ASC::dense)
if(ASC_CPP_EXAMPLES_ENABLE_LAPACK)
  target_compile_definitions(
    asc_cpp_example_lapack_array_io PRIVATE ASC_CPP_EXAMPLE_LAPACK=1
  )
  target_link_libraries(
    asc_cpp_example_lapack_array_io PRIVATE ASC::dense_lapack
  )
endif()

foreach(_asc_fixture IN ITEMS a.asc b.asc corrupt.asc)
  configure_file(
    "${_asc_fixture}" "${CMAKE_CURRENT_BINARY_DIR}/${_asc_fixture}" COPYONLY
  )
endforeach()

set(_asc_array_io_routes native)
if(ASC_CPP_EXAMPLES_ENABLE_LAPACK)
  list(APPEND _asc_array_io_routes reference)
endif()
foreach(_asc_route IN LISTS _asc_array_io_routes)
  add_test(
    NAME "asc_cpp.example.lapack_array_io.${_asc_route}"
    COMMAND asc_cpp_example_lapack_array_io "${_asc_route}"
      "${CMAKE_CURRENT_BINARY_DIR}/a.asc"
      "${CMAKE_CURRENT_BINARY_DIR}/b.asc"
      "${CMAKE_CURRENT_BINARY_DIR}/corrupt.asc"
      "${CMAKE_CURRENT_BINARY_DIR}/${_asc_route}-solution.ascb"
      --truncate
  )
  if(WIN32)
    set_tests_properties(
      "asc_cpp.example.lapack_array_io.${_asc_route}"
      PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:ASC::dense>"
    )
  endif()
endforeach()
