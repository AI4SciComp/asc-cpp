include_guard(GLOBAL)

function(asc_cpp_validate_test_workspace)
  set(_one_value WORK_DIR ROOT GUARD OUTPUT_VARIABLE)
  cmake_parse_arguments(
    WORKSPACE
    ""
    "${_one_value}"
    ""
    ${ARGN}
  )
  foreach(_required IN ITEMS WORK_DIR ROOT GUARD OUTPUT_VARIABLE)
    if(NOT DEFINED WORKSPACE_${_required}
       OR "${WORKSPACE_${_required}}" STREQUAL "")
      message(FATAL_ERROR
        "asc_cpp_validate_test_workspace requires ${_required}"
      )
    endif()
  endforeach()

  set(_root "${WORKSPACE_ROOT}")
  cmake_path(IS_ABSOLUTE _root _root_is_absolute)
  if(NOT _root_is_absolute OR NOT IS_DIRECTORY "${_root}")
    message(FATAL_ERROR
      "The ASCCpp test workspace root must be an existing absolute "
      "directory: '${_root}'"
    )
  endif()
  file(REAL_PATH "${_root}" _root_real)

  set(_guard "${WORKSPACE_GUARD}")
  cmake_path(IS_ABSOLUTE _guard _guard_is_absolute)
  if(NOT _guard_is_absolute OR NOT EXISTS "${_guard}"
     OR IS_DIRECTORY "${_guard}" OR IS_SYMLINK "${_guard}")
    message(FATAL_ERROR
      "The ASCCpp test workspace guard must be a regular absolute file: "
      "'${_guard}'"
    )
  endif()
  file(REAL_PATH "${_guard}" _guard_real)
  cmake_path(GET _guard_real PARENT_PATH _guard_parent)
  cmake_path(
    COMPARE
    "${_guard_parent}"
    EQUAL
    "${_root_real}"
    _guard_is_in_root
  )
  if(NOT _guard_is_in_root)
    message(FATAL_ERROR
      "The ASCCpp test workspace guard is outside its root: '${_guard_real}'"
    )
  endif()
  file(READ "${_guard_real}" _guard_contents LIMIT 256)
  if(NOT _guard_contents MATCHES
     "^ASCCpp dedicated test workspace root\n")
    message(FATAL_ERROR
      "The ASCCpp test workspace guard has invalid contents: '${_guard_real}'"
    )
  endif()

  set(_work_dir "${WORKSPACE_WORK_DIR}")
  cmake_path(IS_ABSOLUTE _work_dir _work_dir_is_absolute)
  if(NOT _work_dir_is_absolute)
    message(FATAL_ERROR
      "The ASCCpp test work directory must be absolute: '${_work_dir}'"
    )
  endif()
  cmake_path(NORMAL_PATH _work_dir OUTPUT_VARIABLE _work_normal)
  set(_work_existing_ancestor "${_work_normal}")
  while(TRUE)
    if(IS_SYMLINK "${_work_existing_ancestor}")
      message(FATAL_ERROR
        "The ASCCpp test work directory must not contain a symlink: "
        "'${_work_existing_ancestor}'"
      )
    endif()
    if(EXISTS "${_work_existing_ancestor}")
      break()
    endif()
    cmake_path(GET _work_existing_ancestor PARENT_PATH _work_parent)
    cmake_path(
      COMPARE
      "${_work_existing_ancestor}"
      EQUAL
      "${_work_parent}"
      _work_reached_root
    )
    if(_work_reached_root)
      message(FATAL_ERROR
        "The ASCCpp test work directory has no existing ancestor: "
        "'${_work_normal}'"
      )
    endif()
    set(_work_existing_ancestor "${_work_parent}")
  endwhile()
  file(REAL_PATH "${_work_existing_ancestor}" _work_existing_real)
  cmake_path(
    RELATIVE_PATH
    _work_normal
    BASE_DIRECTORY "${_work_existing_ancestor}"
    OUTPUT_VARIABLE _work_suffix
  )
  if(_work_suffix STREQUAL ".")
    set(_work_real "${_work_existing_real}")
  else()
    cmake_path(
      APPEND
      _work_existing_real
      "${_work_suffix}"
      OUTPUT_VARIABLE _work_real
    )
  endif()
  cmake_path(NORMAL_PATH _work_real)
  cmake_path(
    COMPARE
    "${_work_real}"
    EQUAL
    "${_root_real}"
    _work_is_root
  )
  cmake_path(IS_PREFIX _root_real "${_work_real}" NORMALIZE _work_is_below_root)
  if(_work_is_root OR NOT _work_is_below_root)
    message(FATAL_ERROR
      "Refusing unsafe ASCCpp test work directory '${_work_real}'; it must "
      "be a strict descendant of dedicated root '${_root_real}'"
    )
  endif()

  set("${WORKSPACE_OUTPUT_VARIABLE}" "${_work_real}" PARENT_SCOPE)
endfunction()

function(asc_cpp_prepare_test_workspace)
  set(_one_value WORK_DIR ROOT GUARD)
  cmake_parse_arguments(
    WORKSPACE
    ""
    "${_one_value}"
    ""
    ${ARGN}
  )
  asc_cpp_validate_test_workspace(
    WORK_DIR "${WORKSPACE_WORK_DIR}"
    ROOT "${WORKSPACE_ROOT}"
    GUARD "${WORKSPACE_GUARD}"
    OUTPUT_VARIABLE _validated_work_dir
  )
  file(REMOVE_RECURSE "${_validated_work_dir}")
  file(MAKE_DIRECTORY "${_validated_work_dir}")
endfunction()

function(asc_cpp_remove_test_workspace)
  set(_one_value WORK_DIR ROOT GUARD)
  cmake_parse_arguments(
    WORKSPACE
    ""
    "${_one_value}"
    ""
    ${ARGN}
  )
  asc_cpp_validate_test_workspace(
    WORK_DIR "${WORKSPACE_WORK_DIR}"
    ROOT "${WORKSPACE_ROOT}"
    GUARD "${WORKSPACE_GUARD}"
    OUTPUT_VARIABLE _validated_work_dir
  )
  file(REMOVE_RECURSE "${_validated_work_dir}")
endfunction()
