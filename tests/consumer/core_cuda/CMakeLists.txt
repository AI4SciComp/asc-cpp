cmake_minimum_required(VERSION 3.25)

project(ASCCppCoreCudaConsumer LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS core_cuda)

if(NOT ASCCpp_VERSION VERSION_EQUAL "0.9.0"
   OR NOT ASCCpp_core_FOUND
   OR NOT ASCCpp_core_cuda_FOUND)
  message(FATAL_ERROR "Expected ASCCpp 0.9.0 core_cuda closure.")
endif()
foreach(_required IN ITEMS core core_cuda)
  if(NOT TARGET "ASC::${_required}")
    message(FATAL_ERROR "Core CUDA lookup omitted ASC::${_required}.")
  endif()
endforeach()
foreach(_forbidden IN ITEMS
    utilities
    expression
    dense
    sparse
    random
    random_dense
    random_sparse
    cpp
    dense_cuda
    sparse_cuda
    random_cuda
    random_dense_cuda
    random_sparse_cuda
)
  if(TARGET "ASC::${_forbidden}")
    message(FATAL_ERROR "Core CUDA lookup imported ASC::${_forbidden}.")
  endif()
endforeach()
get_target_property(_type ASC::core_cuda TYPE)
if(DEFINED ASCCPP_EXPECT_LIBRARY_TYPE
   AND NOT _type STREQUAL ASCCPP_EXPECT_LIBRARY_TYPE)
  message(FATAL_ERROR
    "Expected core_cuda type '${ASCCPP_EXPECT_LIBRARY_TYPE}', found '${_type}'."
  )
endif()
get_target_property(_links ASC::core_cuda INTERFACE_LINK_LIBRARIES)
if(NOT "ASC::core" IN_LIST _links)
  message(FATAL_ERROR "Core CUDA closure omits ASC::core: '${_links}'.")
endif()
foreach(_link IN LISTS _links)
  if(_link MATCHES "^ASC::" AND NOT _link STREQUAL "ASC::core")
    message(FATAL_ERROR "Core CUDA has an unexpected ASC edge: '${_link}'.")
  endif()
endforeach()

add_executable(asc_cpp_core_cuda_consumer main.cc)
set_target_properties(
  asc_cpp_core_cuda_consumer
  PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)
target_link_libraries(asc_cpp_core_cuda_consumer PRIVATE ASC::core_cuda)
