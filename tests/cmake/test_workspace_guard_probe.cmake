cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS WORK_DIR ROOT GUARD)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/PrepareTestWorkspace.cmake")
if(VALIDATE_ONLY)
  asc_cpp_validate_test_workspace(
    WORK_DIR "${WORK_DIR}"
    ROOT "${ROOT}"
    GUARD "${GUARD}"
    OUTPUT_VARIABLE _validated
  )
else()
  asc_cpp_prepare_test_workspace(
    WORK_DIR "${WORK_DIR}"
    ROOT "${ROOT}"
    GUARD "${GUARD}"
  )
endif()
