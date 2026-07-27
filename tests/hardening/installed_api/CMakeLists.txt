cmake_minimum_required(VERSION 3.25)

project(ASCCppM8InstalledApiProbe LANGUAGES CXX)

if(NOT DEFINED ASCCPP_EXPECT_CUDA)
  message(FATAL_ERROR "ASCCPP_EXPECT_CUDA is required.")
endif()

set(_components cpp)
if(ASCCPP_EXPECT_CUDA)
  list(APPEND _components
    core_cuda
    dense_cuda
    sparse_cuda
    random_cuda
    random_dense_cuda
    random_sparse_cuda
  )
endif()
find_package(
  ASCCpp 0.9 CONFIG REQUIRED
  COMPONENTS ${_components}
)

if(NOT ASCCpp_VERSION VERSION_EQUAL "0.9.0")
  message(FATAL_ERROR
    "Expected ASCCpp 0.9.0, found '${ASCCpp_VERSION}'."
  )
endif()
foreach(_component IN LISTS _components)
  if(NOT ASCCpp_${_component}_FOUND OR NOT TARGET "ASC::${_component}")
    message(FATAL_ERROR
      "Installed API probe is missing component '${_component}'."
    )
  endif()
endforeach()
if(NOT ASCCPP_EXPECT_CUDA
   AND (CUDAToolkit_FOUND
        OR TARGET CUDA::cudart
        OR TARGET CUDA::cublas
        OR TARGET CUDA::cusparse))
  message(FATAL_ERROR
    "Provider-free installed API lookup discovered CUDAToolkit."
  )
endif()

add_executable(
  asc_cpp_m8_installed_api_probe
  installed_api_contract.cc
)
# Convert package component names to their public target names.
set(_api_targets)
foreach(_component IN LISTS _components)
  list(APPEND _api_targets "ASC::${_component}")
endforeach()
target_link_libraries(
  asc_cpp_m8_installed_api_probe
  PRIVATE ${_api_targets}
)
set_target_properties(
  asc_cpp_m8_installed_api_probe
  PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)
if(ASCCPP_EXPECT_CUDA)
  target_compile_definitions(
    asc_cpp_m8_installed_api_probe
    PRIVATE ASC_M8_EXPECT_CUDA_HEADERS=1
  )
endif()

enable_testing()
add_test(
  NAME asc_cpp_m8_installed_api_probe
  COMMAND asc_cpp_m8_installed_api_probe
)

unset(_api_targets)
unset(_component)
unset(_components)
