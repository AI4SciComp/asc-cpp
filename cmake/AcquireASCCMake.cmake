include_guard(GLOBAL)

# ASCCMake 0.1.0 is a build-only dependency. Prefer an explicitly supplied or
# installed package. The fallback is immutable and anonymously downloadable
# from the public AI4SciComp/asc-cmake repository.
find_package(ASCCMake 0.1.0 EXACT CONFIG QUIET)

if(NOT ASCCMake_FOUND AND ASC_CPP_FETCH_ASCCMAKE)
  include(FetchContent)
  set(ASC_CMAKE_BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(ASC_CMAKE_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    ASCCMake
    URL
      "https://api.github.com/repos/AI4SciComp/asc-cmake/tarball/8a7dcbad3a97267cce59810aff24de800a3497a7"
    URL_HASH
      "SHA256=67765391bef06c6c9a1a0c43e934d0db7a9c52876e66e0cf644042eeb0c2a5c9"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    EXCLUDE_FROM_ALL
  )
  FetchContent_MakeAvailable(ASCCMake)
  set(
    ASCCMake_DIR
    "${asccmake_BINARY_DIR}"
    CACHE PATH "Directory containing ASCCMakeConfig.cmake" FORCE
  )
endif()

find_package(ASCCMake 0.1.0 EXACT CONFIG REQUIRED)
if(NOT ASCCMake_VERSION VERSION_EQUAL "0.1.0")
  message(FATAL_ERROR
    "ASCCpp 0.9.0 requires ASCCMake 0.1.0 exactly; found "
    "'${ASCCMake_VERSION}'."
  )
endif()
