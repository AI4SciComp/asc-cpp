cmake_minimum_required(VERSION 3.25)

project(ASCCppM8PackageMetadataProbe LANGUAGES CXX)

foreach(_required IN ITEMS
    ASCCPP_REQUEST_VERSION
    ASCCPP_EXPECT_FOUND
    ASCCPP_EXPECT_CUDA
)
  if(NOT DEFINED "${_required}")
    message(FATAL_ERROR "${_required} is required.")
  endif()
endforeach()
set(_expect_cuda "${ASCCPP_EXPECT_CUDA}")

set(_known_components
  core
  utilities
  expression
  dense
  sparse
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
set(_optional_components "${_known_components}")
list(REMOVE_ITEM _optional_components core)

if(ASCCPP_REQUEST_EXACT)
  find_package(
    ASCCpp "${ASCCPP_REQUEST_VERSION}" EXACT CONFIG QUIET
    COMPONENTS core
    OPTIONAL_COMPONENTS ${_optional_components}
  )
else()
  find_package(
    ASCCpp "${ASCCPP_REQUEST_VERSION}" CONFIG QUIET
    COMPONENTS core
    OPTIONAL_COMPONENTS ${_optional_components}
  )
endif()

if(ASCCPP_EXPECT_FOUND AND NOT ASCCpp_FOUND)
  message(FATAL_ERROR
    "Expected request '${ASCCPP_REQUEST_VERSION}' to find ASCCpp: "
    "${ASCCpp_NOT_FOUND_MESSAGE}"
  )
elseif(NOT ASCCPP_EXPECT_FOUND AND ASCCpp_FOUND)
  message(FATAL_ERROR
    "Expected request '${ASCCPP_REQUEST_VERSION}' to reject ASCCpp "
    "'${ASCCpp_VERSION}'."
  )
endif()

if(NOT ASCCPP_EXPECT_FOUND)
  if(DEFINED ASCCpp_CONSIDERED_VERSIONS
     AND NOT "0.9.0" IN_LIST ASCCpp_CONSIDERED_VERSIONS)
    message(FATAL_ERROR
      "The rejected request did not consider the 0.9.0 candidate: "
      "'${ASCCpp_CONSIDERED_VERSIONS}'."
    )
  endif()
  return()
endif()

if(NOT ASCCpp_VERSION VERSION_EQUAL "0.9.0")
  message(FATAL_ERROR
    "Expected ASCCpp_VERSION 0.9.0, found '${ASCCpp_VERSION}'."
  )
endif()

set(_actual_known_components "${ASCCpp_KNOWN_COMPONENTS}")
list(SORT _known_components)
list(SORT _actual_known_components)
if(NOT _actual_known_components STREQUAL _known_components)
  message(FATAL_ERROR
    "ASCCpp_KNOWN_COMPONENTS differs from the M8 oracle: "
    "'${ASCCpp_KNOWN_COMPONENTS}'."
  )
endif()

set(_available_components
  core
  utilities
  expression
  dense
  sparse
  random
  random_dense
  random_sparse
  cpp
)
if(_expect_cuda)
  list(APPEND _available_components
    core_cuda
    dense_cuda
    sparse_cuda
    random_cuda
    random_dense_cuda
    random_sparse_cuda
  )
endif()
set(_actual_available_components "${ASCCpp_AVAILABLE_COMPONENTS}")
list(SORT _available_components)
list(SORT _actual_available_components)
if(NOT _actual_available_components STREQUAL _available_components)
  message(FATAL_ERROR
    "ASCCpp_AVAILABLE_COMPONENTS differs from the M8 oracle: "
    "'${ASCCpp_AVAILABLE_COMPONENTS}'."
  )
endif()

foreach(_component IN LISTS _known_components)
  if(NOT DEFINED "ASCCpp_${_component}_FOUND")
    message(FATAL_ERROR
      "Package metadata omitted ASCCpp_${_component}_FOUND."
    )
  endif()
  if(_component IN_LIST _available_components)
    if(NOT ASCCpp_${_component}_FOUND)
      message(FATAL_ERROR
        "Available component '${_component}' was not reported found."
      )
    elseif(NOT TARGET "ASC::${_component}")
      message(FATAL_ERROR
        "Found component '${_component}' omitted ASC::${_component}."
      )
    endif()
  elseif(ASCCpp_${_component}_FOUND)
    message(FATAL_ERROR
      "Unavailable component '${_component}' was reported found."
    )
  elseif(TARGET "ASC::${_component}")
    message(FATAL_ERROR
      "Unavailable component '${_component}' created an imported target."
    )
  endif()
endforeach()

if(_expect_cuda)
  foreach(_target IN ITEMS CUDA::cudart CUDA::cublas CUDA::cusparse)
    if(NOT TARGET "${_target}")
      message(FATAL_ERROR
        "The complete CUDA closure omitted '${_target}'."
      )
    endif()
  endforeach()
elseif(CUDAToolkit_FOUND
       OR TARGET CUDA::cudart
       OR TARGET CUDA::cublas
       OR TARGET CUDA::cusparse)
  message(FATAL_ERROR
    "A CUDA-disabled package exposed CUDAToolkit state."
  )
endif()

add_executable(asc_cpp_m8_package_metadata_probe core_smoke.cc)
target_link_libraries(asc_cpp_m8_package_metadata_probe PRIVATE ASC::core)
set_target_properties(
  asc_cpp_m8_package_metadata_probe
  PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)

unset(_actual_available_components)
unset(_actual_known_components)
unset(_available_components)
unset(_component)
unset(_expect_cuda)
unset(_known_components)
unset(_optional_components)
unset(_required)
unset(_target)
