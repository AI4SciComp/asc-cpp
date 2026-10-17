include_guard(GLOBAL)

if(PROJECT_IS_TOP_LEVEL)
  if(DEFINED BUILD_TESTING)
    set(_asc_cpp_build_testing_default "${BUILD_TESTING}")
  else()
    set(_asc_cpp_build_testing_default ON)
  endif()
  set(_asc_cpp_install_default ON)
else()
  set(_asc_cpp_build_testing_default OFF)
  set(_asc_cpp_install_default OFF)
endif()

option(
  ASC_CPP_BUILD_TESTING
  "Build the ASCCpp verification suite"
  "${_asc_cpp_build_testing_default}"
)
option(
  ASC_CPP_INSTALL
  "Install the ASCCpp package"
  "${_asc_cpp_install_default}"
)
option(
  ASC_CPP_BUILD_DOCUMENTATION
  "Build the ASCCpp API documentation"
  OFF
)
option(
  ASC_CPP_FETCH_ASCCMAKE
  "Fetch the exact ASCCMake 0.1.0 source when no package is available"
  "${PROJECT_IS_TOP_LEVEL}"
)
option(
  ASC_CPP_ENABLE_CUDA
  "Build all implemented ASCCpp CUDA provider facets"
  OFF
)
option(ASC_CPP_ENABLE_LAPACK
  "Build the explicit incremental Dense-owned Reference-LAPACK facet" OFF)
option(ASC_CPP_ENABLE_EXPERIMENTAL_ROBUST_PPSVX
  "Build explicitly selected experimental first-party RobustPpsvx APIs" OFF)
if(ASC_CPP_ENABLE_EXPERIMENTAL_ROBUST_PPSVX AND NOT ASC_CPP_ENABLE_LAPACK)
  message(FATAL_ERROR
    "Experimental RobustPpsvx uses the explicit Dense LAPACK context; enable ASC_CPP_ENABLE_LAPACK.")
endif()
set(ASC_CPP_LAPACK_ROOT "" CACHE PATH "Explicit prepared Reference-LAPACK prefix")
set(ASC_CPP_LAPACK_ATTESTATION "" CACHE FILEPATH
  "Exact external Reference-LAPACK build attestation")
set(ASC_CPP_LAPACK_INTEGER_BITS "32" CACHE STRING
  "Reference provider integer ABI: 32 (LP64) or 64 (true ILP64)")
set_property(CACHE ASC_CPP_LAPACK_INTEGER_BITS PROPERTY STRINGS 32 64)
set(ASC_CPP_LAPACK_RUNTIME_LIBRARIES "" CACHE STRING
  "Explicit absolute paths to the attested GNU Fortran and quadmath runtimes")
set(ASC_CPP_LAPACK_EVIDENCE_ROOT "" CACHE PATH
  "External verification artifacts for the strict full-profile gate")
option(ASC_CPP_LAPACK_REQUIRE_FULL_PROFILE
  "Require all frozen upstream rows and modes to be implemented and verified" OFF)
if(ASC_CPP_LAPACK_REQUIRE_FULL_PROFILE AND NOT ASC_CPP_ENABLE_LAPACK)
  message(FATAL_ERROR "The full LAPACK profile requires ASC_CPP_ENABLE_LAPACK=ON.")
endif()
option(
  ASC_CPP_WARNINGS_AS_ERRORS
  "Treat warnings from ASCCpp-owned targets as errors"
  OFF
)
option(
  ASC_CPP_ENABLE_ADDRESS_SANITIZER
  "Instrument ASCCpp-owned targets with AddressSanitizer"
  OFF
)
option(
  ASC_CPP_ENABLE_UNDEFINED_SANITIZER
  "Instrument ASCCpp-owned targets with UndefinedBehaviorSanitizer"
  OFF
)
option(
  ASC_CPP_ENABLE_THREAD_SANITIZER
  "Instrument ASCCpp-owned targets with ThreadSanitizer"
  OFF
)
option(
  ASC_CPP_ENABLE_LEAK_SANITIZER
  "Instrument ASCCpp-owned targets with LeakSanitizer"
  OFF
)

if(ASC_CPP_ENABLE_THREAD_SANITIZER
   AND (ASC_CPP_ENABLE_ADDRESS_SANITIZER
        OR ASC_CPP_ENABLE_LEAK_SANITIZER))
  message(FATAL_ERROR
    "ThreadSanitizer cannot be combined with AddressSanitizer or "
    "LeakSanitizer."
  )
endif()

set(ASC_CPP_SANITIZER_ARGUMENTS)
if(ASC_CPP_ENABLE_ADDRESS_SANITIZER)
  list(APPEND ASC_CPP_SANITIZER_ARGUMENTS ADDRESS)
endif()
if(ASC_CPP_ENABLE_UNDEFINED_SANITIZER)
  list(APPEND ASC_CPP_SANITIZER_ARGUMENTS UNDEFINED)
endif()
if(ASC_CPP_ENABLE_THREAD_SANITIZER)
  list(APPEND ASC_CPP_SANITIZER_ARGUMENTS THREAD)
endif()
if(ASC_CPP_ENABLE_LEAK_SANITIZER)
  list(APPEND ASC_CPP_SANITIZER_ARGUMENTS LEAK)
endif()

unset(_asc_cpp_build_testing_default)
unset(_asc_cpp_install_default)
