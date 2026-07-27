cmake_minimum_required(VERSION 3.25)

project(ASCCppM8AscXdeTrial LANGUAGES CXX)

set(_approved_asc_xde_commit
  "abcb29b51f22f40afd7f174707b7ccf83c32d4bf"
)
if(NOT DEFINED ASC_XDE_TRIAL_BASELINE
   OR NOT ASC_XDE_TRIAL_BASELINE STREQUAL _approved_asc_xde_commit)
  message(FATAL_ERROR
    "ASC_XDE_TRIAL_BASELINE must identify the approved skeletal asc-xde "
    "commit '${_approved_asc_xde_commit}'."
  )
endif()

# The trial intentionally requests the minimal public closure for its
# storage/evaluation workflow.
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS dense)
if(NOT ASCCpp_VERSION VERSION_EQUAL "0.9.0")
  message(FATAL_ERROR
    "The asc-xde trial requires ASCCpp 0.9.0, found '${ASCCpp_VERSION}'."
  )
endif()
foreach(_required_component IN ITEMS core expression dense)
  if(NOT ASCCpp_${_required_component}_FOUND
     OR NOT TARGET "ASC::${_required_component}")
    message(FATAL_ERROR
      "The dense closure omitted '${_required_component}'."
    )
  endif()
endforeach()
foreach(_forbidden_target IN ITEMS
    ASC::utilities
    ASC::sparse
    ASC::random
    ASC::random_dense
    ASC::random_sparse
    ASC::cpp
    ASC::core_cuda
    ASC::dense_cuda
    ASC::sparse_cuda
    ASC::random_cuda
    ASC::random_dense_cuda
    ASC::random_sparse_cuda
    CUDA::cudart
    CUDA::cublas
    CUDA::cusparse
)
  if(TARGET "${_forbidden_target}")
    message(FATAL_ERROR
      "The minimal CPU trial imported forbidden target "
      "'${_forbidden_target}'."
    )
  endif()
endforeach()
if(CUDAToolkit_FOUND)
  message(FATAL_ERROR
    "The minimal CPU asc-xde trial discovered CUDAToolkit."
  )
endif()

add_executable(asc_cpp_m8_asc_xde_trial asc_xde_trial.cc)
target_link_libraries(asc_cpp_m8_asc_xde_trial PRIVATE ASC::dense)
target_compile_definitions(
  asc_cpp_m8_asc_xde_trial
  PRIVATE "ASC_XDE_TRIAL_BASELINE=\"${ASC_XDE_TRIAL_BASELINE}\""
)
set_target_properties(
  asc_cpp_m8_asc_xde_trial
  PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)

enable_testing()
add_test(
  NAME asc_cpp_m8_asc_xde_trial
  COMMAND asc_cpp_m8_asc_xde_trial
)

unset(_approved_asc_xde_commit)
unset(_forbidden_target)
unset(_required_component)
