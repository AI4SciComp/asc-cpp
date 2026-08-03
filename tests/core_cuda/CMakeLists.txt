function(_asc_core_cuda_configure_test target)
  target_link_libraries("${target}" PRIVATE ASC::core_cuda)
  target_include_directories("${target}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  if(ASC_CPP_INTERNAL_ENABLE_CUDA_RUNTIME_TEST_HOOKS)
    target_compile_definitions(
      "${target}"
      PRIVATE ASC_CPP_CUDA_RUNTIME_TEST_HOOKS
    )
  endif()
  asc_target_enable_cxx20(TARGET "${target}")
  if(ASC_CPP_WARNINGS_AS_ERRORS)
    asc_target_enable_warnings(TARGET "${target}" AS_ERRORS)
  else()
    asc_target_enable_warnings(TARGET "${target}")
  endif()
  if(ASC_CPP_SANITIZER_ARGUMENTS)
    asc_target_enable_sanitizers(
      TARGET "${target}"
      ${ASC_CPP_SANITIZER_ARGUMENTS}
    )
  endif()
endfunction()

foreach(_source IN ITEMS
    core_cuda_native_state_test.cc
    core_cuda_runtime_test.cc
    core_cuda_validation_test.cc
)
  get_filename_component(_name "${_source}" NAME_WE)
  set(_target "asc_core_cuda_${_name}")
  add_executable("${_target}" "${_source}")
  _asc_core_cuda_configure_test("${_target}")
  asc_register_test(
    NAME "asc_cpp.core_cuda.${_name}"
    TARGET "${_target}"
    LABELS "core_cuda;cuda;gpu;cuda-core-dense;runtime"
  )
  set_tests_properties(
    "asc_cpp.core_cuda.${_name}"
    PROPERTIES SKIP_RETURN_CODE 77
  )
  add_test(
    NAME "asc_cpp.core_cuda.${_name}.forced_no_device"
    COMMAND "$<TARGET_FILE:${_target}>"
  )
  set_tests_properties(
    "asc_cpp.core_cuda.${_name}.forced_no_device"
    PROPERTIES
      ENVIRONMENT "ASC_CPP_TEST_FORCE_NO_CUDA_DEVICE=1"
      LABELS "core_cuda;cuda;gpu;hardening;no-device"
      SKIP_RETURN_CODE 77
  )
endforeach()

target_link_libraries(
  asc_core_cuda_core_cuda_native_state_test
  PRIVATE CUDA::cudart
)

unset(_name)
unset(_source)
unset(_target)
