add_executable(asc_core_cuda_test core_cuda_test.cc)
target_include_directories(
  asc_core_cuda_test
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(asc_core_cuda_test PRIVATE ASC::core_cuda)
asc_target_enable_cxx20(TARGET asc_core_cuda_test)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(TARGET asc_core_cuda_test AS_ERRORS)
else()
  asc_target_enable_warnings(TARGET asc_core_cuda_test)
endif()
if(ASC_CPP_SANITIZER_ARGUMENTS)
  asc_target_enable_sanitizers(
    TARGET asc_core_cuda_test
    ${ASC_CPP_SANITIZER_ARGUMENTS}
  )
endif()
asc_register_test(
  NAME asc_cpp.core_cuda.runtime
  TARGET asc_core_cuda_test
  LABELS "core_cuda;gpu;milestone-6;runtime"
)
