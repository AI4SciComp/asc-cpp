cmake_minimum_required(VERSION 3.25)

project(ASCCppFoundationSubprojectConsumer LANGUAGES CXX)

if(NOT DEFINED ASCCPP_SOURCE_DIR OR ASCCPP_SOURCE_DIR STREQUAL "")
  message(FATAL_ERROR "ASCCPP_SOURCE_DIR is required.")
endif()

if(DEFINED ASCCPP_ENABLE_CUDA AND ASCCPP_ENABLE_CUDA)
  set(ASC_CPP_ENABLE_CUDA ON CACHE BOOL "" FORCE)
endif()
add_subdirectory("${ASCCPP_SOURCE_DIR}" asc-cpp)

if(ASC_CPP_BUILD_TESTING)
  message(FATAL_ERROR "Subproject use must default testing off.")
endif()
if(ASC_CPP_INSTALL)
  message(FATAL_ERROR "Subproject use must default installation off.")
endif()
foreach(_required_target IN ITEMS
    ASC::core
    ASC::utilities
    ASC::expression
    ASC::dense
    ASC::sparse
    ASC::random
    ASC::random_dense
    ASC::random_sparse
    ASC::cpp
)
  if(NOT TARGET "${_required_target}")
    message(FATAL_ERROR
      "Subproject use did not create ${_required_target}."
    )
  endif()
endforeach()
set(_provider_targets
  ASC::core_cuda
  ASC::dense_cuda
  ASC::sparse_cuda
  ASC::random_cuda
  ASC::random_dense_cuda
  ASC::random_sparse_cuda
)
if(ASC_CPP_ENABLE_CUDA)
  foreach(_provider_target IN LISTS _provider_targets)
    if(NOT TARGET "${_provider_target}")
      message(FATAL_ERROR
        "CUDA-enabled subproject use omitted ${_provider_target}."
      )
    endif()
  endforeach()
else()
  foreach(_provider_target IN LISTS _provider_targets)
    if(TARGET "${_provider_target}")
      message(FATAL_ERROR
        "CUDA-disabled subproject use created ${_provider_target}."
      )
    endif()
  endforeach()
endif()
unset(_provider_targets)

add_executable(asc_cpp_subproject_consumer main.cc)
target_link_libraries(
  asc_cpp_subproject_consumer
  PRIVATE ASC::cpp
)
