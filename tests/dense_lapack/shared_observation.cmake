# GNU ELF --wrap on an executable cannot rewrite a call inside an existing DSO.
# Bind the existing test wrappers in a test-only DSO made from the production
# PIC objects. Actual provider/runtime DSOs are unchanged and uninstrumented.
# Ordinary unwrapped and installed consumers keep the production ASC library.
if(BUILD_SHARED_LIBS)
  if(NOT TARGET asc_dense_lapack_objects)
    message(FATAL_ERROR "Shared LAPACK observation requires production PIC objects")
  endif()
  get_property(_asc_tests DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
  foreach(_test IN LISTS _asc_tests)
    get_target_property(_options "${_test}" LINK_OPTIONS)
    set(_wrapped)
    foreach(_option IN LISTS _options)
      if(_option MATCHES "--wrap=")
        list(APPEND _wrapped "${_option}")
      endif()
    endforeach()
    if(NOT _wrapped)
      continue()
    endif()
    get_target_property(_links "${_test}" LINK_LIBRARIES)
    if(NOT "ASC::dense_lapack" IN_LIST _links)
      continue()
    endif()
    set(_observed "asc_dense_lapack_observed_${_test}")
    add_library("${_observed}" SHARED
      "$<TARGET_OBJECTS:asc_dense_lapack_objects>")
    target_link_libraries("${_observed}" PUBLIC ASC::dense
      PRIVATE ASC_INTERNAL_LAPACK::reference)
    target_link_options("${_observed}" PRIVATE ${_wrapped} "LINKER:-z,text")
    if(ASC_CPP_SANITIZER_ARGUMENTS)
      asc_target_enable_sanitizers(TARGET "${_observed}"
        ${ASC_CPP_SANITIZER_ARGUMENTS})
    endif()
    list(TRANSFORM _links REPLACE "^ASC::dense_lapack$" "${_observed}")
    set_property(TARGET "${_test}" PROPERTY LINK_LIBRARIES "${_links}")
    # The observation DSO resolves __wrap_* in its owning test executable.
    target_link_options("${_test}" PRIVATE "LINKER:--export-dynamic")
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shared-observation/${_test}.txt"
      CONTENT "production=$<TARGET_FILE:asc_dense_lapack>\nobservation=$<TARGET_FILE:${_observed}>\nobjects=$<TARGET_OBJECTS:asc_dense_lapack_objects>\nwrapped=${_wrapped}\nprovider_instrumented=false\nruntime_instrumented=false\n")
  endforeach()
  unset(_asc_tests)
  unset(_test)
  unset(_options)
  unset(_option)
  unset(_wrapped)
  unset(_links)
  unset(_observed)
endif()
