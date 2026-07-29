function(_asc_random_dense_configure_test target)
  target_link_libraries("${target}" PRIVATE ASC::random_dense)
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
    dense_generation_test.cc
    thread_partition_test.cc
)
  get_filename_component(_name "${_source}" NAME_WE)
  set(_target "asc_random_dense_${_name}")
  add_executable("${_target}" "${_source}")
  _asc_random_dense_configure_test("${_target}")
  asc_register_test(
    NAME "asc_cpp.random_dense.${_name}"
    TARGET "${_target}"
    LABELS "random;random_dense;milestone-5;runtime"
  )
endforeach()
target_link_libraries(
  asc_random_dense_thread_partition_test
  PRIVATE Threads::Threads
)

add_executable(
  asc_random_dense_allocation_test
  allocation_test.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc"
)
_asc_random_dense_configure_test(asc_random_dense_allocation_test)
asc_register_test(
  NAME asc_cpp.random_dense.allocation_test
  TARGET asc_random_dense_allocation_test
  LABELS "random;random_dense;allocation;milestone-5;runtime"
)

foreach(_negative_source IN ITEMS
    m5_negative_const_dense.cc
    m5_negative_integral_dense.cc
    m5_negative_random_base_visibility.cc
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

unset(_name)
unset(_negative_compiled)
unset(_negative_name)
unset(_negative_output)
unset(_negative_source)
unset(_source)
unset(_target)
