cmake_minimum_required(VERSION 3.25)

project(ASCCppRepeatedComponentConsumer LANGUAGES CXX)

if(NOT DEFINED ENABLE_CUDA)
  message(FATAL_ERROR "ENABLE_CUDA is required.")
endif()

function(assert_exact_link_interface target)
  get_target_property(_actual "${target}" INTERFACE_LINK_LIBRARIES)
  if(_actual STREQUAL "_actual-NOTFOUND")
    set(_actual)
  endif()
  set(_expected ${ARGN})
  if(NOT _actual STREQUAL _expected)
    message(FATAL_ERROR
      "${target} has link interface '${_actual}'; expected '${_expected}'."
    )
  endif()
endfunction()

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS core)
if(NOT TARGET ASC::core)
  message(FATAL_ERROR "The first lookup did not import ASC::core.")
endif()
foreach(_unexpected IN ITEMS
    ASC::expression
    ASC::dense
    ASC::core_cuda
    ASC::dense_cuda
    CUDA::cudart
    CUDA::cublas
)
  if(TARGET "${_unexpected}")
    message(FATAL_ERROR
      "The first core-only lookup imported '${_unexpected}'."
    )
  endif()
endforeach()

if(ENABLE_CUDA)
  find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS dense_cuda)
  set(_expected
    ASC::core
    ASC::expression
    ASC::dense
    ASC::core_cuda
    ASC::dense_cuda
    CUDA::cudart
    CUDA::cublas
  )
else()
  find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS dense)
  set(_expected ASC::core ASC::expression ASC::dense)
endif()
foreach(_target IN LISTS _expected)
  if(NOT TARGET "${_target}")
    message(FATAL_ERROR
      "The second lookup did not import '${_target}'."
    )
  endif()
endforeach()
foreach(_unrelated IN ITEMS
    ASC::utilities
    ASC::random
    ASC::sparse
    ASC::random_dense
    ASC::random_sparse
    ASC::cpp
    ASC::sparse_cuda
    ASC::random_cuda
    ASC::random_dense_cuda
    ASC::random_sparse_cuda
)
  if(TARGET "${_unrelated}")
    message(FATAL_ERROR
      "The repeated dense lookup imported unrelated target '${_unrelated}'."
    )
  endif()
endforeach()

assert_exact_link_interface(ASC::expression ASC::core)
assert_exact_link_interface(ASC::dense ASC::core ASC::expression)
if(ENABLE_CUDA)
  get_target_property(_core_cuda_type ASC::core_cuda TYPE)
  get_target_property(_dense_cuda_type ASC::dense_cuda TYPE)
  if(_core_cuda_type STREQUAL "STATIC_LIBRARY")
    assert_exact_link_interface(
      ASC::core_cuda
      ASC::core
      "$<LINK_ONLY:CUDA::cudart>"
    )
  else()
    assert_exact_link_interface(ASC::core_cuda ASC::core)
  endif()
  if(_dense_cuda_type STREQUAL "STATIC_LIBRARY")
    assert_exact_link_interface(
      ASC::dense_cuda
      ASC::dense
      ASC::core_cuda
      "$<LINK_ONLY:CUDA::cublas>"
    )
  else()
    assert_exact_link_interface(ASC::dense_cuda ASC::dense ASC::core_cuda)
  endif()
endif()

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS utilities)
if(NOT TARGET ASC::utilities)
  message(FATAL_ERROR
    "The third lookup did not import ASC::utilities."
  )
endif()
assert_exact_link_interface(ASC::utilities ASC::core)

add_executable(asc_cpp_repeated_component_consumer main.cc)
target_link_libraries(
  asc_cpp_repeated_component_consumer
  PRIVATE ASC::dense ASC::utilities
)
set_target_properties(
  asc_cpp_repeated_component_consumer
  PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)
