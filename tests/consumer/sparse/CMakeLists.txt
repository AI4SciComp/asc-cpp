cmake_minimum_required(VERSION 3.25)

project(ASCCppSparseConsumer LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS sparse)

if(NOT ASCCpp_VERSION VERSION_EQUAL "0.9.0"
   OR NOT ASCCpp_sparse_FOUND
   OR NOT ASCCpp_expression_FOUND
   OR NOT ASCCpp_core_FOUND)
  message(FATAL_ERROR
    "Expected ASCCpp 0.9.0 sparse with core/expression closure."
  )
endif()
foreach(_required IN ITEMS core expression sparse)
  if(NOT TARGET "ASC::${_required}")
    message(FATAL_ERROR "Sparse lookup omitted ASC::${_required}.")
  endif()
endforeach()
foreach(_forbidden IN ITEMS
    utilities
    dense
    random
    random_dense
    random_sparse
    cpp
    core_cuda
    dense_cuda
    sparse_cuda
    random_cuda
    random_dense_cuda
    random_sparse_cuda
)
  if(TARGET "ASC::${_forbidden}")
    message(FATAL_ERROR "Sparse-only lookup imported ASC::${_forbidden}.")
  endif()
endforeach()
get_target_property(_type ASC::sparse TYPE)
if(DEFINED ASCCPP_EXPECT_LIBRARY_TYPE
   AND NOT _type STREQUAL ASCCPP_EXPECT_LIBRARY_TYPE)
  message(FATAL_ERROR
    "Expected sparse type '${ASCCPP_EXPECT_LIBRARY_TYPE}', found '${_type}'."
  )
endif()
get_target_property(_links ASC::sparse INTERFACE_LINK_LIBRARIES)
if(NOT _links STREQUAL "ASC::core;ASC::expression")
  message(FATAL_ERROR "Sparse dependencies differ: '${_links}'.")
endif()

add_executable(asc_cpp_sparse_consumer main.cc)
set_target_properties(
  asc_cpp_sparse_consumer
  PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)
target_link_libraries(asc_cpp_sparse_consumer PRIVATE ASC::sparse)
