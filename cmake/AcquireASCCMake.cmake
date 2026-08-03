include_guard(GLOBAL)

# ASCCMake 0.1.0 is a build-only dependency. Prefer an explicitly supplied or
# installed package. The fallback is immutable and becomes anonymously
# downloadable when AI4SciComp/asc-cmake is public.
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
      "SHA256=73299eca4b80b8a5571622636fe12c88f67602eba4e99f24368d5405b9bc0021"
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
