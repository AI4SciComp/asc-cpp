cmake_minimum_required(VERSION 3.25)

project(ASCCppAggregateConsumer LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS cpp)

foreach(_required_target IN ITEMS
    ASC::core
    ASC::utilities
    ASC::expression
    ASC::random
    ASC::dense
    ASC::sparse
    ASC::random_dense
    ASC::random_sparse
    ASC::cpp
)
  if(NOT TARGET "${_required_target}")
    message(FATAL_ERROR "The cpp package omitted '${_required_target}'.")
  endif()
endforeach()
foreach(_forbidden_target IN ITEMS
    ASC::core_cuda
    ASC::dense_cuda
    ASC::sparse_cuda
    ASC::random_cuda
    ASC::random_dense_cuda
    ASC::random_sparse_cuda
)
  if(TARGET "${_forbidden_target}")
    message(FATAL_ERROR "The cpp package imported '${_forbidden_target}'.")
  endif()
endforeach()

if(NOT ASCCpp_VERSION VERSION_EQUAL "0.9.0")
  message(FATAL_ERROR "Expected ASCCpp 0.9.0, found '${ASCCpp_VERSION}'.")
endif()
if(DEFINED ASCCPP_EXPECT_LIBRARY_TYPE
   AND NOT ASCCPP_EXPECT_LIBRARY_TYPE STREQUAL "")
  get_target_property(_library_type ASC::cpp TYPE)
  if(NOT _library_type STREQUAL ASCCPP_EXPECT_LIBRARY_TYPE)
    message(FATAL_ERROR
      "Expected ASC::cpp type '${ASCCPP_EXPECT_LIBRARY_TYPE}', found "
      "'${_library_type}'."
    )
  endif()
endif()

add_executable(asc_cpp_cpp_consumer main.cc)
target_compile_features(asc_cpp_cpp_consumer PRIVATE cxx_std_20)
set_target_properties(asc_cpp_cpp_consumer PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(asc_cpp_cpp_consumer PRIVATE ASC::cpp)

unset(_forbidden_target)
unset(_library_type)
unset(_required_target)
