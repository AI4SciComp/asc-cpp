cmake_minimum_required(VERSION 3.25)

project(ASCCppRandomSparseCudaConsumer LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS random_sparse_cuda)

foreach(_required_target IN ITEMS
    ASC::core
    ASC::expression
    ASC::random
    ASC::sparse
    ASC::random_sparse
    ASC::core_cuda
    ASC::random_cuda
    ASC::random_sparse_cuda
    CUDA::cudart
)
  if(NOT TARGET "${_required_target}")
    message(FATAL_ERROR
      "The random_sparse_cuda package omitted '${_required_target}'."
    )
  endif()
endforeach()
foreach(_forbidden_target IN ITEMS
    ASC::utilities
    ASC::dense
    ASC::random_dense
    ASC::cpp
    ASC::dense_cuda
    ASC::sparse_cuda
    ASC::random_dense_cuda
)
  if(TARGET "${_forbidden_target}")
    message(FATAL_ERROR
      "The random_sparse_cuda closure imported '${_forbidden_target}'."
    )
  endif()
endforeach()

if(NOT ASCCpp_VERSION VERSION_EQUAL "0.9.0")
  message(FATAL_ERROR "Expected ASCCpp 0.9.0, found '${ASCCpp_VERSION}'.")
endif()
get_target_property(_links ASC::random_sparse_cuda INTERFACE_LINK_LIBRARIES)
foreach(_required_link IN ITEMS
    ASC::random_sparse
    ASC::random_cuda
    ASC::core_cuda
)
  if(NOT "${_required_link}" IN_LIST _links)
    message(FATAL_ERROR
      "ASC::random_sparse_cuda omitted direct link '${_required_link}': "
      "'${_links}'."
    )
  endif()
endforeach()
if(DEFINED ASCCPP_EXPECT_LIBRARY_TYPE
   AND NOT ASCCPP_EXPECT_LIBRARY_TYPE STREQUAL "")
  get_target_property(_library_type ASC::random_sparse_cuda TYPE)
  if(NOT _library_type STREQUAL ASCCPP_EXPECT_LIBRARY_TYPE)
    message(FATAL_ERROR
      "Expected ASC::random_sparse_cuda type "
      "'${ASCCPP_EXPECT_LIBRARY_TYPE}', found '${_library_type}'."
    )
  endif()
endif()

add_executable(asc_cpp_random_sparse_cuda_consumer main.cc)
target_compile_features(asc_cpp_random_sparse_cuda_consumer PRIVATE cxx_std_20)
set_target_properties(
  asc_cpp_random_sparse_cuda_consumer
  PROPERTIES CXX_EXTENSIONS OFF
)
target_link_libraries(
  asc_cpp_random_sparse_cuda_consumer
  PRIVATE ASC::random_sparse_cuda
)
