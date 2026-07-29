cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS WORK_DIR ROOT GUARD)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/PrepareTestWorkspace.cmake")
asc_cpp_remove_test_workspace(
  WORK_DIR "${WORK_DIR}"
  ROOT "${ROOT}"
  GUARD "${GUARD}"
)
