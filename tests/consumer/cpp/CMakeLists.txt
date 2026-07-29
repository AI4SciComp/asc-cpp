cmake_minimum_required(VERSION 3.25)

project(ASCCppAggregateConsumer LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED)

if(NOT ASCCpp_VERSION VERSION_EQUAL "0.9.0")
  message(FATAL_ERROR "Expected ASCCpp 0.9.0.")
endif()
foreach(_required
        IN ITEMS
        core
        utilities
        expression
        random
        dense
        sparse
        random_dense
        random_sparse
        cpp)
  if(NOT ASCCpp_${_required}_FOUND OR NOT TARGET "ASC::${_required}")
    message(FATAL_ERROR
      "No-component lookup omitted ASCCpp/ASC::${_required}."
    )
  endif()
endforeach()
foreach(_provider IN ITEMS
    core_cuda
    dense_cuda
    sparse_cuda
    random_cuda
    random_dense_cuda
    random_sparse_cuda
)
  if(TARGET "ASC::${_provider}")
    message(FATAL_ERROR
      "Provider-free no-component lookup imported ASC::${_provider}."
    )
  endif()
endforeach()
get_target_property(_type ASC::cpp TYPE)
if(NOT _type STREQUAL "INTERFACE_LIBRARY")
  message(FATAL_ERROR "Expected cpp interface target, found '${_type}'.")
endif()
get_target_property(_links ASC::cpp INTERFACE_LINK_LIBRARIES)
set(_expected
  "ASC::core;ASC::utilities;ASC::expression;ASC::dense;ASC::sparse;ASC::random;ASC::random_dense;ASC::random_sparse"
)
if(NOT _links STREQUAL _expected)
  message(FATAL_ERROR "cpp aggregate dependencies differ: '${_links}'.")
endif()

add_executable(asc_cpp_aggregate_consumer main.cc)
set_target_properties(
  asc_cpp_aggregate_consumer
  PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)
target_link_libraries(asc_cpp_aggregate_consumer PRIVATE ASC::cpp)
