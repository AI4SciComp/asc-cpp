cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED ASC_CPP_HARDENING_SOURCE_DIR)
  cmake_path(
    ABSOLUTE_PATH CMAKE_CURRENT_LIST_DIR
    NORMALIZE
    OUTPUT_VARIABLE _asc_cpp_tools_dir
  )
  cmake_path(GET _asc_cpp_tools_dir PARENT_PATH _asc_cpp_tools_parent)
  cmake_path(
    GET _asc_cpp_tools_parent
    PARENT_PATH
    ASC_CPP_HARDENING_SOURCE_DIR
  )
endif()
cmake_path(
  ABSOLUTE_PATH ASC_CPP_HARDENING_SOURCE_DIR
  NORMALIZE
  OUTPUT_VARIABLE _asc_cpp_source_dir
)

if(NOT IS_DIRECTORY "${_asc_cpp_source_dir}/include/asc")
  message(FATAL_ERROR
    "ASC_CPP_HARDENING_SOURCE_DIR does not contain include/asc: "
    "'${_asc_cpp_source_dir}'."
  )
endif()

if(DEFINED ASC_CPP_HARDENING_HEADER_BASELINE)
  set(_asc_cpp_header_baseline "${ASC_CPP_HARDENING_HEADER_BASELINE}")
else()
  set(_asc_cpp_header_baseline
    "${_asc_cpp_source_dir}/abi/public-headers.sha256"
  )
endif()
if(DEFINED ASC_CPP_HARDENING_TARGET_BASELINE)
  set(_asc_cpp_target_baseline "${ASC_CPP_HARDENING_TARGET_BASELINE}")
else()
  set(_asc_cpp_target_baseline
    "${_asc_cpp_source_dir}/abi/targets-and-components.txt"
  )
endif()

foreach(_asc_cpp_baseline IN ITEMS
    "${_asc_cpp_header_baseline}"
    "${_asc_cpp_target_baseline}"
)
  if(NOT EXISTS "${_asc_cpp_baseline}")
    message(FATAL_ERROR
      "Required hardening baseline does not exist: "
      "'${_asc_cpp_baseline}'."
    )
  endif()
endforeach()
unset(_asc_cpp_baseline)

function(_asc_cpp_hardening_assert_external_output path)
  cmake_path(
    ABSOLUTE_PATH path
    NORMALIZE
    OUTPUT_VARIABLE _absolute_output
  )
  cmake_path(
    IS_PREFIX _asc_cpp_source_dir
    "${_absolute_output}"
    NORMALIZE
    _inside_source
  )
  if(_inside_source)
    message(FATAL_ERROR
      "Hardening reports must be outside the source tree: "
      "'${_absolute_output}'."
    )
  endif()
  cmake_path(GET _absolute_output PARENT_PATH _output_parent)
  if(NOT IS_DIRECTORY "${_output_parent}")
    message(FATAL_ERROR
      "Hardening report parent does not exist: '${_output_parent}'."
    )
  endif()
endfunction()

function(_asc_cpp_hardening_collect_headers include_dir output_variable)
  if(NOT IS_DIRECTORY "${include_dir}/asc")
    message(FATAL_ERROR
      "Public include directory does not contain asc/: '${include_dir}'."
    )
  endif()

  file(
    GLOB_RECURSE _headers
    LIST_DIRECTORIES FALSE
    RELATIVE "${include_dir}"
    "${include_dir}/asc/*.h"
  )
  list(SORT _headers)

  set(_report)
  foreach(_header IN LISTS _headers)
    if(IS_SYMLINK "${include_dir}/${_header}")
      message(FATAL_ERROR
        "Public header baseline does not accept symlinks: '${_header}'."
      )
    endif()
    file(READ "${include_dir}/${_header}" _contents)
    string(REPLACE "\r\n" "\n" _contents "${_contents}")
    string(REPLACE "\r" "\n" _contents "${_contents}")
    string(SHA256 _sha256 "${_contents}")
    string(APPEND _report "${_sha256}  ${_header}\n")
  endforeach()
  set("${output_variable}" "${_report}" PARENT_SCOPE)
endfunction()

file(READ "${_asc_cpp_header_baseline}" _expected_headers)
string(REPLACE "\r\n" "\n" _expected_headers "${_expected_headers}")
string(REPLACE "\r" "\n" _expected_headers "${_expected_headers}")
if(NOT _expected_headers MATCHES "\n$")
  string(APPEND _expected_headers "\n")
endif()

_asc_cpp_hardening_collect_headers(
  "${_asc_cpp_source_dir}/include"
  _source_headers
)
if(NOT _source_headers STREQUAL _expected_headers)
  message(FATAL_ERROR
    "The source public-header surface differs from "
    "'${_asc_cpp_header_baseline}'."
  )
endif()

if(DEFINED ASC_CPP_HARDENING_INSTALLED_INCLUDE_DIR)
  cmake_path(
    ABSOLUTE_PATH ASC_CPP_HARDENING_INSTALLED_INCLUDE_DIR
    NORMALIZE
    OUTPUT_VARIABLE _installed_include_dir
  )
  _asc_cpp_hardening_collect_headers(
    "${_installed_include_dir}"
    _installed_headers
  )
  set(_expected_cpu_headers)
  set(_expected_cuda_headers)
  set(_expected_lapack_headers)
  string(REPLACE "\n" ";" _expected_header_lines "${_expected_headers}")
  foreach(_header_line IN LISTS _expected_header_lines)
    if(NOT _header_line STREQUAL ""
       AND NOT _header_line MATCHES "  asc/.+/providers/")
      string(APPEND _expected_cpu_headers "${_header_line}\n")
      string(APPEND _expected_cuda_headers "${_header_line}\n")
      string(APPEND _expected_lapack_headers "${_header_line}\n")
    elseif(_header_line MATCHES "  asc/dense/providers/lapack")
      string(APPEND _expected_lapack_headers "${_header_line}\n")
    elseif(NOT _header_line STREQUAL "")
      string(APPEND _expected_cuda_headers "${_header_line}\n")
    endif()
  endforeach()
  if(_installed_headers STREQUAL _expected_headers)
    set(_installed_status "checked-full")
  elseif(_installed_headers STREQUAL _expected_cpu_headers)
    set(_installed_status "checked-cpu")
  elseif(_installed_headers STREQUAL _expected_cuda_headers)
    set(_installed_status "checked-cpu-and-cuda")
  elseif(_installed_headers STREQUAL _expected_lapack_headers)
    set(_installed_status "checked-cpu-and-lapack")
  else()
    message(FATAL_ERROR
      "The installed public-header surface differs from the exact provider "
      "projections of '${_asc_cpp_header_baseline}'."
    )
  endif()
else()
  set(_installed_status "not-requested")
endif()

include("${_asc_cpp_source_dir}/cmake/ASCCppComponents.cmake")
set(_actual_components)
foreach(_component IN LISTS ASC_CPP_KNOWN_COMPONENTS)
  set(_dependencies "${ASC_CPP_COMPONENT_${_component}_DEPENDENCIES}")
  string(JOIN "," _dependency_csv ${_dependencies})
  string(APPEND _actual_components
    "component|${_component}|${_dependency_csv}\n"
  )
endforeach()

file(STRINGS "${_asc_cpp_target_baseline}" _target_baseline_lines)
set(_expected_components)
foreach(_line IN LISTS _target_baseline_lines)
  if(_line MATCHES "^component\\|")
    string(APPEND _expected_components "${_line}\n")
  endif()
endforeach()
if(NOT _actual_components STREQUAL _expected_components)
  message(FATAL_ERROR
    "The component inventory or dependency closure differs from "
    "'${_asc_cpp_target_baseline}'."
  )
endif()

list(LENGTH ASC_CPP_KNOWN_COMPONENTS _component_count)
string(REGEX MATCHALL "\n" _header_line_breaks "${_source_headers}")
list(LENGTH _header_line_breaks _header_count)

set(_report "format|asc-cpp-public-surface-v1\n")
string(APPEND _report "status|passed\n")
string(APPEND _report "public-header-count|${_header_count}\n")
string(APPEND _report "component-count|${_component_count}\n")
string(APPEND _report
  "installed-header-equivalence|${_installed_status}\n"
)
string(APPEND _report "${_actual_components}")
string(APPEND _report "${_source_headers}")

if(DEFINED ASC_CPP_HARDENING_OUTPUT)
  _asc_cpp_hardening_assert_external_output(
    "${ASC_CPP_HARDENING_OUTPUT}"
  )
  file(WRITE "${ASC_CPP_HARDENING_OUTPUT}" "${_report}")
endif()

message(STATUS
  "ASCCpp public surface passed: ${_header_count} headers, "
  "${_component_count} components, installed=${_installed_status}."
)
