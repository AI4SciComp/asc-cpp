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
  "Build ASCCpp Milestone 8 hardening and predecessor tests"
  "${_asc_cpp_build_testing_default}"
)
option(
  ASC_CPP_INSTALL
  "Install the ASCCpp Milestone 8 components and package metadata"
  "${_asc_cpp_install_default}"
)
option(
  ASC_CPP_WARNINGS_AS_ERRORS
  "Treat warnings from ASCCpp-owned targets as errors"
  OFF
)
option(
  ASC_CPP_ENABLE_CUDA
  "Build all approved ASCCpp CUDA provider facets"
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
