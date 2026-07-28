function(_asc_random_sparse_configure_test target)
  target_link_libraries("${target}" PRIVATE ASC::random_sparse)
  target_include_directories("${target}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
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
    sparse_generation_test.cc
    thread_reproducibility_test.cc
)
  get_filename_component(_name "${_source}" NAME_WE)
  set(_target "asc_random_sparse_${_name}")
  add_executable("${_target}" "${_source}")
  _asc_random_sparse_configure_test("${_target}")
  asc_register_test(
    NAME "asc_cpp.random_sparse.${_name}"
    TARGET "${_target}"
    LABELS "random;random_sparse;milestone-5;runtime"
  )
endforeach()
target_link_libraries(
  asc_random_sparse_thread_reproducibility_test
  PRIVATE Threads::Threads
)

add_executable(
  asc_random_sparse_allocation_test
  allocation_test.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc"
)
_asc_random_sparse_configure_test(asc_random_sparse_allocation_test)
asc_register_test(
  NAME asc_cpp.random_sparse.allocation_test
  TARGET asc_random_sparse_allocation_test
  LABELS "random;random_sparse;allocation;milestone-5;runtime"
)

foreach(_negative_source IN ITEMS
    m5_negative_copy_generation.cc
    m5_negative_integral_sparse.cc
)
  get_filename_component(_negative_name "${_negative_source}" NAME_WE)
  try_compile(
    _negative_compiled
    "${CMAKE_CURRENT_BINARY_DIR}/negative/${_negative_name}"
    SOURCES "${PROJECT_SOURCE_DIR}/tests/compile/${_negative_source}"
    CMAKE_FLAGS
      "-DCMAKE_CXX_EXTENSIONS:BOOL=OFF"
      "-DCMAKE_CXX_STANDARD:STRING=20"
      "-DCMAKE_CXX_STANDARD_REQUIRED:BOOL=ON"
      "-DINCLUDE_DIRECTORIES:STRING=${PROJECT_SOURCE_DIR}/include"
    OUTPUT_VARIABLE _negative_output
  )
  if(_negative_compiled)
    message(FATAL_ERROR
      "Expected ${_negative_source} to fail compilation.\n"
      "${_negative_output}"
    )
  endif()
endforeach()

add_executable(
  asc_random_storage_benchmark
  "${PROJECT_SOURCE_DIR}/benchmarks/random_storage/random_storage_benchmark.cc"
  "${PROJECT_SOURCE_DIR}/benchmarks/random_storage/allocation_probe.cc"
)
target_link_libraries(
  asc_random_storage_benchmark
  PRIVATE ASC::random_dense ASC::random_sparse
)
target_include_directories(
  asc_random_storage_benchmark
  PRIVATE "${PROJECT_SOURCE_DIR}/tests/random_cuda"
)
asc_target_enable_cxx20(TARGET asc_random_storage_benchmark)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(TARGET asc_random_storage_benchmark AS_ERRORS)
else()
  asc_target_enable_warnings(TARGET asc_random_storage_benchmark)
endif()
if(ASC_CPP_SANITIZER_ARGUMENTS)
  asc_target_enable_sanitizers(
    TARGET asc_random_storage_benchmark
    ${ASC_CPP_SANITIZER_ARGUMENTS}
  )
endif()
asc_register_test(
  NAME asc_cpp.random_storage.benchmark
  TARGET asc_random_storage_benchmark
  LABELS "benchmark;random;milestone-5;performance"
)
set_tests_properties(
  asc_cpp.random_storage.benchmark
  PROPERTIES TIMEOUT 120
)

unset(_name)
unset(_negative_compiled)
unset(_negative_name)
unset(_negative_output)
unset(_negative_source)
unset(_source)
unset(_target)
