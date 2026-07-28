function(_asc_random_configure_test target)
  target_link_libraries("${target}" PRIVATE ASC::random)
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

foreach(_source IN ITEMS distribution_test.cc engine_test.cc)
  get_filename_component(_name "${_source}" NAME_WE)
  set(_target "asc_random_${_name}")
  add_executable("${_target}" "${_source}")
  _asc_random_configure_test("${_target}")
  asc_register_test(
    NAME "asc_cpp.random.${_name}"
    TARGET "${_target}"
    LABELS "milestone-2;random;runtime"
  )
endforeach()
