cmake_minimum_required(VERSION 3.25)

project(ASCCppRandomSparseCudaConsumer LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS random_sparse_cuda)

foreach(_required IN ITEMS
    core
    expression
    sparse
    random
    random_sparse
    core_cuda
    random_cuda
    random_sparse_cuda
)
  if(NOT ASCCpp_${_required}_FOUND OR NOT TARGET "ASC::${_required}")
    message(FATAL_ERROR "Random Sparse CUDA lookup omitted ASC::${_required}.")
  endif()
endforeach()
foreach(_forbidden IN ITEMS
    utilities
    dense
    random_dense
    cpp
    dense_cuda
    sparse_cuda
    random_dense_cuda
)
  if(TARGET "ASC::${_forbidden}")
    message(FATAL_ERROR
      "Random Sparse CUDA lookup imported ASC::${_forbidden}."
    )
  endif()
endforeach()
get_target_property(_type ASC::random_sparse_cuda TYPE)
if(DEFINED ASCCPP_EXPECT_LIBRARY_TYPE
   AND NOT _type STREQUAL ASCCPP_EXPECT_LIBRARY_TYPE)
  message(FATAL_ERROR
    "Expected random_sparse_cuda type '${ASCCPP_EXPECT_LIBRARY_TYPE}', found "
    "'${_type}'."
  )
endif()
get_target_property(_links ASC::random_sparse_cuda INTERFACE_LINK_LIBRARIES)
foreach(_required_link IN ITEMS
    ASC::random_sparse
    ASC::random_cuda
    ASC::core_cuda
)
  if(NOT _required_link IN_LIST _links)
    message(FATAL_ERROR
      "Random Sparse CUDA closure omits ${_required_link}: '${_links}'."
    )
  endif()
endforeach()
foreach(_link IN LISTS _links)
  if(_link MATCHES "^ASC::"
     AND NOT _link STREQUAL "ASC::random_sparse"
     AND NOT _link STREQUAL "ASC::random_cuda"
     AND NOT _link STREQUAL "ASC::core_cuda")
    message(FATAL_ERROR
      "Random Sparse CUDA has an unexpected ASC edge: '${_link}'."
    )
  endif()
endforeach()

add_executable(asc_cpp_random_sparse_cuda_consumer main.cc)
set_target_properties(
  asc_cpp_random_sparse_cuda_consumer
  PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)
target_link_libraries(
  asc_cpp_random_sparse_cuda_consumer
  PRIVATE ASC::random_sparse_cuda
)
