cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS SOURCE_DIR BINARY_DIR ROOT GUARD)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

set(_probe "${CMAKE_CURRENT_LIST_DIR}/test_workspace_guard_probe.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/PrepareTestWorkspace.cmake")

function(_expect_rejected name candidate)
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}"
      "-DWORK_DIR:PATH=${candidate}"
      "-DROOT:PATH=${ROOT}"
      "-DGUARD:FILEPATH=${GUARD}"
      -DVALIDATE_ONLY:BOOL=ON
      -P "${_probe}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
  )
  if(_result EQUAL 0)
    message(FATAL_ERROR
      "Unsafe workspace case '${name}' was accepted: '${candidate}'"
    )
  endif()
endfunction()

file(MAKE_DIRECTORY "${ROOT}/guard-test-outside")
file(WRITE "${ROOT}/guard-test-outside/sentinel.txt" "preserve\n")

_expect_rejected(filesystem_root "/")
_expect_rejected(source_root "${SOURCE_DIR}")
_expect_rejected(build_root "${BINARY_DIR}")
_expect_rejected(workspace_root "${ROOT}")
cmake_path(GET ROOT PARENT_PATH _workspace_parent)
_expect_rejected(workspace_parent "${_workspace_parent}")
_expect_rejected(
  dot_dot_traversal
  "${ROOT}/guard-test-child/../../guard-test-outside"
)

set(_outside "${SOURCE_DIR}")
set(_symlink "${ROOT}/guard-test-symlink")
file(REMOVE "${_symlink}")
file(CREATE_LINK "${_outside}" "${_symlink}" SYMBOLIC RESULT _link_result)
if(_link_result STREQUAL "0")
  _expect_rejected(symlink_escape "${_symlink}")
  _expect_rejected(symlink_descendant_escape "${_symlink}/new child")
  file(REMOVE "${_symlink}")
endif()
if(NOT EXISTS "${_outside}/CMakeLists.txt"
   OR NOT EXISTS "${ROOT}/guard-test-outside/sentinel.txt")
  message(FATAL_ERROR "Workspace rejection changed an external sentinel")
endif()

set(_valid "${ROOT}/guard-test-valid")
file(MAKE_DIRECTORY "${_valid}")
file(WRITE "${_valid}/stale.txt" "remove\n")
execute_process(
  COMMAND
    "${CMAKE_COMMAND}"
    "-DWORK_DIR:PATH=${_valid}"
    "-DROOT:PATH=${ROOT}"
    "-DGUARD:FILEPATH=${GUARD}"
    -DVALIDATE_ONLY:BOOL=OFF
    -P "${_probe}"
  RESULT_VARIABLE _valid_result
  OUTPUT_VARIABLE _valid_stdout
  ERROR_VARIABLE _valid_stderr
)
if(NOT _valid_result EQUAL 0 OR NOT IS_DIRECTORY "${_valid}"
   OR EXISTS "${_valid}/stale.txt")
  message(FATAL_ERROR
    "Valid dedicated workspace was not cleaned safely.\n"
    "stdout:\n${_valid_stdout}\n"
    "stderr:\n${_valid_stderr}"
  )
endif()

file(
  GLOB_RECURSE
  _test_cmake_sources
  LIST_DIRECTORIES FALSE
  "${SOURCE_DIR}/tests/*CMakeLists.txt"
  "${SOURCE_DIR}/tests/*.cmake"
)
foreach(_source IN LISTS _test_cmake_sources)
  if(_source STREQUAL
     "${SOURCE_DIR}/tests/cmake/PrepareTestWorkspace.cmake"
     OR _source STREQUAL
        "${SOURCE_DIR}/tests/cmake/test_workspace_guard_test.cmake")
    continue()
  endif()
  file(READ "${_source}" _contents)
  if(_contents MATCHES "file[ \t\r\n]*\\([ \t\r\n]*REMOVE_RECURSE"
     OR _contents MATCHES
        "-E[ \t\r\n;]+rm[ \t\r\n;]+-[fFrR]*[rR][fFrR]*[ \t\r\n;]"
     OR _contents MATCHES
        "[ \t\r\n;]rm[ \t\r\n;]+-[fFrR]*[rR][fFrR]*[ \t\r\n;]"
     OR _contents MATCHES
        "-E[ \t\r\n;]+remove_directory[ \t\r\n;]")
    message(FATAL_ERROR
      "Test CMake source performs recursive removal outside the guarded "
      "workspace helper: ${_source}"
    )
  endif()
endforeach()

file(READ "${SOURCE_DIR}/tests/hardening/CMakeLists.txt" _hardening_cmake)
string(
  REGEX MATCHALL
  "RemoveTestWorkspace\\.cmake"
  _guarded_cleanup_registrations
  "${_hardening_cmake}"
)
list(LENGTH _guarded_cleanup_registrations _guarded_cleanup_count)
if(NOT _guarded_cleanup_count EQUAL 3)
  message(FATAL_ERROR
    "Expected exactly three guarded hardening cleanup registrations, found "
    "${_guarded_cleanup_count}"
  )
endif()

asc_cpp_remove_test_workspace(
  WORK_DIR "${_valid}"
  ROOT "${ROOT}"
  GUARD "${GUARD}"
)
asc_cpp_remove_test_workspace(
  WORK_DIR "${ROOT}/guard-test-outside"
  ROOT "${ROOT}"
  GUARD "${GUARD}"
)
