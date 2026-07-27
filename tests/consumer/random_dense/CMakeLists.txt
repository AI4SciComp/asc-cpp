cmake_minimum_required(VERSION 3.25)

project(ASCCppRandomDenseConsumer LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS random_dense)

foreach(_required_target IN ITEMS
    ASC::core
    ASC::expression
    ASC::random
    ASC::dense
    ASC::random_dense
)
  if(NOT TARGET "${_required_target}")
    message(FATAL_ERROR
      "The random_dense package omitted '${_required_target}'."
    )
  endif()
endforeach()
foreach(_forbidden_target IN ITEMS
    ASC::utilities
    ASC::sparse
    ASC::random_sparse
    ASC::cpp
    ASC::core_cuda
    ASC::dense_cuda
    ASC::sparse_cuda
    ASC::random_cuda
    ASC::random_dense_cuda
    ASC::random_sparse_cuda
)
  if(TARGET "${_forbidden_target}")
    message(FATAL_ERROR
      "The random_dense package imported '${_forbidden_target}'."
    )
  endif()
endforeach()

if(NOT ASCCpp_VERSION VERSION_EQUAL "0.9.0")
  message(FATAL_ERROR "Expected ASCCpp 0.9.0, found '${ASCCpp_VERSION}'.")
endif()
if(DEFINED ASCCPP_EXPECT_LIBRARY_TYPE
   AND NOT ASCCPP_EXPECT_LIBRARY_TYPE STREQUAL "")
  get_target_property(_library_type ASC::random_dense TYPE)
  if(NOT _library_type STREQUAL ASCCPP_EXPECT_LIBRARY_TYPE)
    message(FATAL_ERROR
      "Expected ASC::random_dense type '${ASCCPP_EXPECT_LIBRARY_TYPE}', found "
      "'${_library_type}'."
    )
  endif()
endif()

add_executable(asc_cpp_random_dense_consumer main.cc)
target_compile_features(asc_cpp_random_dense_consumer PRIVATE cxx_std_20)
set_target_properties(
  asc_cpp_random_dense_consumer
  PROPERTIES CXX_EXTENSIONS OFF
)
target_link_libraries(
  asc_cpp_random_dense_consumer
  PRIVATE ASC::random_dense
)

unset(_forbidden_target)
unset(_library_type)
unset(_required_target)
