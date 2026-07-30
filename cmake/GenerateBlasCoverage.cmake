cmake_minimum_required(VERSION 3.25)

foreach(_required_variable IN ITEMS SOURCE_DIR BLAS_MANIFEST OUTPUT_REPORT)
  if(NOT DEFINED "${_required_variable}"
     OR "${${_required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${_required_variable} is required.")
  endif()
endforeach()

if(NOT EXISTS "${BLAS_MANIFEST}")
  message(FATAL_ERROR "BLAS manifest does not exist: ${BLAS_MANIFEST}")
endif()

file(READ "${BLAS_MANIFEST}" _manifest)
file(STRINGS "${BLAS_MANIFEST}" _manifest_lines ENCODING UTF-8)

function(_extract_quoted key output)
  string(REGEX MATCH "(^|\n)${key}: \"([^\"]*)\"" _match "${_manifest}")
  if(NOT _match)
    message(FATAL_ERROR "BLAS manifest omits ${key}.")
  endif()
  set("${output}" "${CMAKE_MATCH_2}" PARENT_SCOPE)
endfunction()

function(_extract_integer key output)
  string(REGEX MATCH "(^|\n)${key}: ([0-9]+)" _match "${_manifest}")
  if(NOT _match)
    message(FATAL_ERROR "BLAS manifest omits integer ${key}.")
  endif()
  set("${output}" "${CMAKE_MATCH_2}" PARENT_SCOPE)
endfunction()

function(_require_status label value)
  set(_allowed_statuses
    planned
    implemented
    verified
    not-applicable
    blocked
  )
  if(NOT value IN_LIST _allowed_statuses)
    message(FATAL_ERROR
      "${label} has invalid status '${value}'. Allowed statuses: "
      "${_allowed_statuses}."
    )
  endif()
endfunction()

function(_require_source_path label value)
  if("${value}" STREQUAL "")
    message(FATAL_ERROR "${label} requires a repository path.")
  endif()
  if(IS_ABSOLUTE "${value}" OR "${value}" MATCHES "(^|/)\\.\\.(/|$)")
    message(FATAL_ERROR "${label} must be a repository-relative path: ${value}")
  endif()
  if(NOT EXISTS "${SOURCE_DIR}/${value}")
    message(FATAL_ERROR "${label} does not exist: ${value}")
  endif()
  if(IS_DIRECTORY "${SOURCE_DIR}/${value}")
    message(FATAL_ERROR "${label} is not a regular file: ${value}")
  endif()
endfunction()

function(_require_path_pattern label value pattern)
  if(NOT "${value}" MATCHES "${pattern}")
    message(FATAL_ERROR
      "${label} does not match the approved repository location: ${value}"
    )
  endif()
endfunction()

function(_markdown_code value output)
  if("${value}" STREQUAL "")
    set("${output}" "-" PARENT_SCOPE)
  else()
    set("${output}" "`${value}`" PARENT_SCOPE)
  endif()
endfunction()

function(_markdown_link value output)
  if("${value}" STREQUAL "")
    set("${output}" "-" PARENT_SCOPE)
  elseif(EXISTS "${SOURCE_DIR}/${value}")
    set("${output}" "[`${value}`](../${value})" PARENT_SCOPE)
  else()
    set("${output}" "`${value}`" PARENT_SCOPE)
  endif()
endfunction()

function(_markdown_text value output)
  if("${value}" STREQUAL "")
    set("${output}" "-" PARENT_SCOPE)
    return()
  endif()
  string(REPLACE "|" "\\|" _escaped "${value}")
  string(REPLACE "\n" " " _escaped "${_escaped}")
  set("${output}" "${_escaped}" PARENT_SCOPE)
endfunction()

_extract_integer("schema_version" _schema_version)
if(NOT _schema_version EQUAL 1)
  message(FATAL_ERROR "Unsupported BLAS manifest schema ${_schema_version}.")
endif()

_extract_quoted("contract_status" _contract_status)
_extract_quoted("evidence_date" _evidence_date)
_extract_quoted("allowed_statuses" _declared_allowed_statuses)
_extract_quoted("dense_standard" _dense_standard)
_extract_quoted("dense_commit" _dense_commit)
_extract_quoted("dense_quick_reference_sha256" _dense_quick_reference_sha256)
_extract_quoted("sparse_standard" _sparse_standard)
_extract_quoted("sparse_chapter_sha256" _sparse_chapter_sha256)
_extract_quoted("nist_sparse_version" _nist_sparse_version)
_extract_quoted("nist_sparse_sha256" _nist_sparse_sha256)
_extract_quoted("cuda_toolkit" _cuda_toolkit)
_extract_quoted("cublas_version" _cublas_version)
_extract_quoted("cusparse_version" _cusparse_version)
_extract_quoted("dense_inventory_sha256" _expected_dense_inventory_sha256)
_extract_quoted("sparse_inventory_sha256" _expected_sparse_inventory_sha256)
_extract_quoted("coverage_inventory_sha256"
                _expected_coverage_inventory_sha256)
_extract_quoted("dense_to_sparse_crosswalk_sha256"
                _expected_crosswalk_sha256)
_extract_integer("expected_dense_rows" _expected_dense_rows)
_extract_integer("expected_sparse_rows" _expected_sparse_rows)
_extract_integer("expected_total_rows" _expected_total_rows)

set(_frozen_allowed_statuses
  "planned,implemented,verified,not-applicable,blocked"
)
set(_frozen_dense_commit
  "6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca"
)
set(_frozen_dense_quick_reference_sha256
  "fc35d15e2853e56e8771eb1e85cf3af8fed70af89e9cdb60861901c41bf41ae6"
)
set(_frozen_sparse_chapter_sha256
  "50b54c41bc42efe771f7f5d78a64d02a9373dfe051255e3b4366813101375905"
)
set(_frozen_nist_sparse_sha256
  "2567fd5fbef04a7ec4f649e2809245badcf04c3a866c36639089729e7de41d0b"
)
set(_frozen_cuda_toolkit "12.9.1 compiler 12.9.86")
set(_frozen_cublas_version "12.9.1.4")
set(_frozen_cusparse_version "12.5.10.65")
set(_frozen_dense_rows 150)
set(_frozen_sparse_rows 79)
set(_frozen_total_rows 229)
set(_frozen_dense_inventory_sha256
  "3915d69e0a2339ed73c0c63539a9ed60d4fc1f075b0caa1711309ca7d52fb6b3"
)
set(_frozen_sparse_inventory_sha256
  "ae06c937b9dc8a884e57c666f0939b3b0a13e01714021a64aebe9b54125b272e"
)
set(_frozen_coverage_inventory_sha256
  "45778d5dd3639c9f3557ddf3610a9e01442d29b4503f90b41164f7326c9a8ad0"
)
set(_frozen_crosswalk_sha256
  "78dfab7b056c546f6db1ca2f9ef95b78c357b2dec0ade9ee9ead2f7fca39ca64"
)

foreach(_baseline IN ITEMS
    dense_commit
    dense_quick_reference_sha256
    sparse_chapter_sha256
    nist_sparse_sha256
    cuda_toolkit
    cublas_version
    cusparse_version
)
  if(NOT "${_${_baseline}}" STREQUAL "${_frozen_${_baseline}}")
    message(FATAL_ERROR
      "BLAS manifest ${_baseline} is '${_${_baseline}}'; the Issue 5 "
      "contract freezes '${_frozen_${_baseline}}'."
    )
  endif()
endforeach()

foreach(_frozen_declaration IN ITEMS
    allowed_statuses
    dense_rows
    sparse_rows
    total_rows
    dense_inventory_sha256
    sparse_inventory_sha256
    coverage_inventory_sha256
    crosswalk_sha256
)
  if(_frozen_declaration STREQUAL "allowed_statuses")
    set(_declared_value "${_declared_allowed_statuses}")
    set(_frozen_value "${_frozen_allowed_statuses}")
  elseif(_frozen_declaration MATCHES "_rows$")
    set(_declared_value "${_expected_${_frozen_declaration}}")
    set(_frozen_value "${_frozen_${_frozen_declaration}}")
  elseif(_frozen_declaration STREQUAL "crosswalk_sha256")
    set(_declared_value "${_expected_crosswalk_sha256}")
    set(_frozen_value "${_frozen_crosswalk_sha256}")
  else()
    set(_declared_value "${_expected_${_frozen_declaration}}")
    set(_frozen_value "${_frozen_${_frozen_declaration}}")
  endif()
  if(NOT _declared_value STREQUAL _frozen_value)
    message(FATAL_ERROR
      "BLAS manifest ${_frozen_declaration} is '${_declared_value}'; "
      "the Issue 5 contract freezes '${_frozen_value}'."
    )
  endif()
endforeach()

set(_section)
set(_row_active FALSE)
set(_crosswalk_active FALSE)
set(_coverage_keys)
set(_dense_coverage_keys)
set(_sparse_coverage_keys)
set(_dense_families)
set(_crosswalk_families)
set(_crosswalk_keys)
set(_coverage_rows_markdown)
set(_crosswalk_rows_markdown)
set(_coverage_count 0)
set(_dense_count 0)
set(_sparse_count 0)
set(_planned_count 0)
set(_implemented_count 0)
set(_verified_count 0)
set(_not_applicable_count 0)
set(_blocked_count 0)

set(_coverage_fields
  operation
  official_routine
  standard
  level
  module
  scalar_type
  layout
  backend_reference_cpu
  backend_optimized_cpu
  backend_cuda
  public_api
  public_header
  implementation
  cuda_implementation
  test_conformance
  test_cpu_gpu
  test_invalid_input
  status
  notes
)
set(_crosswalk_fields
  dense_operation
  sparse_analogue
  status
  notes
)

macro(_clear_coverage_row)
  foreach(_field IN LISTS _coverage_fields)
    set("_row_${_field}")
  endforeach()
  set(_row_seen_fields)
endmacro()

macro(_finalize_coverage_row)
  if(_row_active)
    set(_sorted_row_fields ${_row_seen_fields})
    set(_sorted_coverage_fields ${_coverage_fields})
    list(SORT _sorted_row_fields)
    list(SORT _sorted_coverage_fields)
    if(NOT _sorted_row_fields STREQUAL _sorted_coverage_fields)
      message(FATAL_ERROR
        "Coverage row '${_row_official_routine}' does not match schema.\n"
        "  required fields: ${_sorted_coverage_fields}\n"
        "  observed fields: ${_sorted_row_fields}"
      )
    endif()

    foreach(_required_field IN ITEMS
        operation
        official_routine
        standard
        level
        module
        scalar_type
        layout
        backend_reference_cpu
        backend_optimized_cpu
        backend_cuda
        status
    )
      if("${_row_${_required_field}}" STREQUAL "")
        message(FATAL_ERROR
          "Coverage row '${_row_official_routine}' omits "
          "${_required_field}."
        )
      endif()
    endforeach()

    if(NOT _row_module STREQUAL "dense"
       AND NOT _row_module STREQUAL "sparse")
      message(FATAL_ERROR
        "Coverage row '${_row_official_routine}' has invalid module "
        "'${_row_module}'."
      )
    endif()

    foreach(_status_field IN ITEMS
        backend_reference_cpu
        backend_optimized_cpu
        backend_cuda
        status
    )
      _require_status(
        "Coverage row '${_row_official_routine}' ${_status_field}"
        "${_row_${_status_field}}"
      )
    endforeach()

    if(NOT _row_backend_optimized_cpu STREQUAL "not-applicable")
      message(FATAL_ERROR
        "Coverage row '${_row_official_routine}' claims optimized CPU state "
        "'${_row_backend_optimized_cpu}', but Issue 5 approves no optimized "
        "CPU provider."
      )
    endif()

    set(_applicable_backend_count 0)
    set(_implemented_backend_count 0)
    set(_verified_backend_count 0)
    set(_blocked_backend_count 0)
    foreach(_backend IN ITEMS reference_cpu optimized_cpu cuda)
      set(_backend_status "${_row_backend_${_backend}}")
      if(NOT _backend_status STREQUAL "not-applicable")
        math(EXPR _applicable_backend_count
          "${_applicable_backend_count} + 1"
        )
      endif()
      if(_backend_status STREQUAL "implemented")
        math(EXPR _implemented_backend_count
          "${_implemented_backend_count} + 1"
        )
      elseif(_backend_status STREQUAL "verified")
        math(EXPR _verified_backend_count "${_verified_backend_count} + 1")
      elseif(_backend_status STREQUAL "blocked")
        math(EXPR _blocked_backend_count "${_blocked_backend_count} + 1")
      endif()
    endforeach()

    if(_row_status STREQUAL "not-applicable")
      if("${_row_notes}" STREQUAL "")
        message(FATAL_ERROR
          "Not-applicable row '${_row_official_routine}' requires notes."
        )
      endif()
      foreach(_backend IN ITEMS reference_cpu optimized_cpu cuda)
        if(NOT _row_backend_${_backend} STREQUAL "not-applicable")
          message(FATAL_ERROR
            "Not-applicable row '${_row_official_routine}' has applicable "
            "${_backend} state '${_row_backend_${_backend}}'."
          )
        endif()
      endforeach()
    else()
      if(_applicable_backend_count EQUAL 0)
        message(FATAL_ERROR
          "Applicable row '${_row_official_routine}' has no applicable "
          "backend."
        )
      endif()
      if(_row_status STREQUAL "planned")
        if(_implemented_backend_count GREATER 0
           OR _verified_backend_count GREATER 0
           OR _blocked_backend_count GREATER 0)
          message(FATAL_ERROR
            "Planned row '${_row_official_routine}' has a backend beyond "
            "the planned state."
          )
        endif()
      elseif(_row_status STREQUAL "implemented")
        math(EXPR _implementation_evidence_count
          "${_implemented_backend_count} + ${_verified_backend_count}"
        )
        if(_implementation_evidence_count EQUAL 0)
          message(FATAL_ERROR
            "Implemented row '${_row_official_routine}' has no implemented "
            "or verified backend."
          )
        endif()
        if(_blocked_backend_count GREATER 0)
          message(FATAL_ERROR
            "Implemented row '${_row_official_routine}' has a blocked "
            "backend and must use aggregate status 'blocked'."
          )
        endif()
      elseif(_row_status STREQUAL "verified")
        if(NOT _verified_backend_count EQUAL _applicable_backend_count)
          message(FATAL_ERROR
            "Verified row '${_row_official_routine}' has "
            "${_verified_backend_count} verified backends out of "
            "${_applicable_backend_count} applicable backends."
          )
        endif()
      elseif(_row_status STREQUAL "blocked")
        if("${_row_notes}" STREQUAL "")
          message(FATAL_ERROR
            "Blocked row '${_row_official_routine}' requires notes."
          )
        endif()
        if(_blocked_backend_count EQUAL 0)
          message(FATAL_ERROR
            "Blocked row '${_row_official_routine}' has no blocked backend."
          )
        endif()
      endif()
    endif()

    math(EXPR _implementation_evidence_count
      "${_implemented_backend_count} + ${_verified_backend_count}"
    )
    if(_implementation_evidence_count GREATER 0)
      if("${_row_public_api}" STREQUAL "")
        message(FATAL_ERROR
          "${_row_status} row '${_row_official_routine}' requires public_api."
        )
      endif()
      _require_source_path(
        "${_row_status} row '${_row_official_routine}' public_header"
        "${_row_public_header}"
      )
      _require_path_pattern(
        "${_row_status} row '${_row_official_routine}' public_header"
        "${_row_public_header}"
        "^include/asc/${_row_module}/.+\\.h$"
      )
      _require_source_path(
        "${_row_status} row '${_row_official_routine}' implementation"
        "${_row_implementation}"
      )
      _require_path_pattern(
        "${_row_status} row '${_row_official_routine}' implementation"
        "${_row_implementation}"
        "^src/${_row_module}/.+\\.(cc|cu)$"
      )
      _require_source_path(
        "${_row_status} row '${_row_official_routine}' conformance test"
        "${_row_test_conformance}"
      )
      _require_path_pattern(
        "${_row_status} row '${_row_official_routine}' conformance test"
        "${_row_test_conformance}"
        "^tests/.+\\.(cc|cu|cmake)$"
      )
      _require_source_path(
        "${_row_status} row '${_row_official_routine}' invalid-input test"
        "${_row_test_invalid_input}"
      )
      _require_path_pattern(
        "${_row_status} row '${_row_official_routine}' invalid-input test"
        "${_row_test_invalid_input}"
        "^tests/.+\\.(cc|cu|cmake)$"
      )
      if(_row_backend_cuda STREQUAL "implemented"
         OR _row_backend_cuda STREQUAL "verified")
        _require_source_path(
          "${_row_status} row '${_row_official_routine}' CUDA implementation"
          "${_row_cuda_implementation}"
        )
        _require_path_pattern(
          "${_row_status} row '${_row_official_routine}' CUDA implementation"
          "${_row_cuda_implementation}"
          "^src/${_row_module}/cuda/.+\\.(cc|cu)$"
        )
        _require_source_path(
          "${_row_status} row '${_row_official_routine}' CPU/GPU test"
          "${_row_test_cpu_gpu}"
        )
        _require_path_pattern(
          "${_row_status} row '${_row_official_routine}' CPU/GPU test"
          "${_row_test_cpu_gpu}"
          "^tests/.+\\.(cc|cu|cmake)$"
        )
      endif()
    endif()

    set(_coverage_key
      "${_row_module}|${_row_standard}|${_row_level}|"
      "${_row_operation}|${_row_official_routine}|${_row_scalar_type}"
    )
    string(CONCAT _coverage_key ${_coverage_key})
    if(_coverage_key IN_LIST _coverage_keys)
      message(FATAL_ERROR "Duplicate coverage key: ${_coverage_key}")
    endif()
    list(APPEND _coverage_keys "${_coverage_key}")
    if(_row_module STREQUAL "dense")
      list(APPEND _dense_coverage_keys "${_coverage_key}")
    else()
      list(APPEND _sparse_coverage_keys "${_coverage_key}")
    endif()

    math(EXPR _coverage_count "${_coverage_count} + 1")
    if(_row_module STREQUAL "dense")
      math(EXPR _dense_count "${_dense_count} + 1")
      list(APPEND _dense_families "${_row_operation}")
    else()
      math(EXPR _sparse_count "${_sparse_count} + 1")
    endif()

    if(_row_status STREQUAL "planned")
      math(EXPR _planned_count "${_planned_count} + 1")
    elseif(_row_status STREQUAL "implemented")
      math(EXPR _implemented_count "${_implemented_count} + 1")
    elseif(_row_status STREQUAL "verified")
      math(EXPR _verified_count "${_verified_count} + 1")
    elseif(_row_status STREQUAL "not-applicable")
      math(EXPR _not_applicable_count "${_not_applicable_count} + 1")
    elseif(_row_status STREQUAL "blocked")
      math(EXPR _blocked_count "${_blocked_count} + 1")
    endif()

    _markdown_code("${_row_official_routine}" _md_routine)
    _markdown_code("${_row_operation}" _md_operation)
    _markdown_code("${_row_scalar_type}" _md_scalar_type)
    _markdown_text("${_row_layout}" _md_layout)
    _markdown_code("${_row_public_api}" _md_public_api)
    _markdown_link("${_row_public_header}" _md_public_header)
    _markdown_link("${_row_implementation}" _md_implementation)
    _markdown_link("${_row_test_conformance}" _md_conformance)
    _markdown_link("${_row_test_cpu_gpu}" _md_cpu_gpu)
    _markdown_link("${_row_test_invalid_input}" _md_invalid_input)
    _markdown_text("${_row_notes}" _md_notes)
    string(APPEND _coverage_rows_markdown
      "| ${_row_module} | ${_row_level} | ${_md_operation} | "
      "${_md_routine} | ${_md_scalar_type} | ${_md_layout} | "
      "${_row_backend_reference_cpu} | ${_row_backend_optimized_cpu} | "
      "${_row_backend_cuda} | **${_row_status}** | ${_md_public_api}<br>"
      "${_md_public_header} | ${_md_implementation} | ${_md_conformance}<br>"
      "${_md_cpu_gpu}<br>${_md_invalid_input} | ${_md_notes} |\n"
    )
  endif()
  set(_row_active FALSE)
  _clear_coverage_row()
endmacro()

macro(_clear_crosswalk_row)
  set(_crosswalk_dense_operation)
  set(_crosswalk_sparse_analogue)
  set(_crosswalk_status)
  set(_crosswalk_notes)
  set(_crosswalk_seen_fields)
endmacro()

macro(_finalize_crosswalk_row)
  if(_crosswalk_active)
    set(_sorted_crosswalk_row_fields ${_crosswalk_seen_fields})
    set(_sorted_crosswalk_fields ${_crosswalk_fields})
    list(SORT _sorted_crosswalk_row_fields)
    list(SORT _sorted_crosswalk_fields)
    if(NOT _sorted_crosswalk_row_fields STREQUAL _sorted_crosswalk_fields)
      message(FATAL_ERROR
        "Dense-to-sparse '${_crosswalk_dense_operation}' does not match "
        "schema.\n"
        "  required fields: ${_sorted_crosswalk_fields}\n"
        "  observed fields: ${_sorted_crosswalk_row_fields}"
      )
    endif()
    if("${_crosswalk_dense_operation}" STREQUAL ""
       OR "${_crosswalk_status}" STREQUAL "")
      message(FATAL_ERROR "Dense-to-sparse crosswalk row is incomplete.")
    endif()
    _require_status(
      "Dense-to-sparse '${_crosswalk_dense_operation}'"
      "${_crosswalk_status}"
    )
    if(_crosswalk_status STREQUAL "not-applicable"
       AND "${_crosswalk_notes}" STREQUAL "")
      message(FATAL_ERROR
        "Not-applicable dense-to-sparse '${_crosswalk_dense_operation}' "
        "requires notes."
      )
    endif()
    if(_crosswalk_dense_operation IN_LIST _crosswalk_families)
      message(FATAL_ERROR
        "Duplicate dense-to-sparse family: ${_crosswalk_dense_operation}"
      )
    endif()
    list(APPEND _crosswalk_families "${_crosswalk_dense_operation}")
    set(_crosswalk_key
      "${_crosswalk_dense_operation}|${_crosswalk_sparse_analogue}|"
      "${_crosswalk_status}"
    )
    string(CONCAT _crosswalk_key ${_crosswalk_key})
    list(APPEND _crosswalk_keys "${_crosswalk_key}")
    _markdown_code("${_crosswalk_dense_operation}" _md_dense_operation)
    _markdown_code("${_crosswalk_sparse_analogue}" _md_sparse_analogue)
    _markdown_text("${_crosswalk_notes}" _md_crosswalk_notes)
    string(APPEND _crosswalk_rows_markdown
      "| ${_md_dense_operation} | ${_md_sparse_analogue} | "
      "**${_crosswalk_status}** | ${_md_crosswalk_notes} |\n"
    )
  endif()
  set(_crosswalk_active FALSE)
  _clear_crosswalk_row()
endmacro()

_clear_coverage_row()
_clear_crosswalk_row()

foreach(_line IN LISTS _manifest_lines)
  if(_line STREQUAL "coverage:")
    _finalize_crosswalk_row()
    set(_section "coverage")
  elseif(_line STREQUAL "dense_to_sparse_crosswalk:")
    _finalize_coverage_row()
    set(_section "crosswalk")
  elseif(_section STREQUAL "coverage"
         AND _line MATCHES "^  - operation: \"([^\"]+)\"$")
    _finalize_coverage_row()
    set(_row_active TRUE)
    set(_row_operation "${CMAKE_MATCH_1}")
    list(APPEND _row_seen_fields operation)
  elseif(_section STREQUAL "coverage"
         AND _line MATCHES "^    ([a-z_]+): \"([^\"]*)\"$")
    set(_field "${CMAKE_MATCH_1}")
    if(NOT _field IN_LIST _coverage_fields)
      message(FATAL_ERROR
        "Coverage row '${_row_official_routine}' has unknown field "
        "'${_field}'."
      )
    endif()
    if(_field IN_LIST _row_seen_fields)
      message(FATAL_ERROR
        "Coverage row '${_row_official_routine}' repeats field '${_field}'."
      )
    endif()
    list(APPEND _row_seen_fields "${_field}")
    set("_row_${_field}" "${CMAKE_MATCH_2}")
  elseif(_section STREQUAL "crosswalk"
         AND _line MATCHES "^  - dense_operation: \"([^\"]+)\"$")
    _finalize_crosswalk_row()
    set(_crosswalk_active TRUE)
    set(_crosswalk_dense_operation "${CMAKE_MATCH_1}")
    list(APPEND _crosswalk_seen_fields dense_operation)
  elseif(_section STREQUAL "crosswalk"
         AND _line MATCHES "^    ([a-z_]+): \"([^\"]*)\"$")
    set(_field "${CMAKE_MATCH_1}")
    if(NOT _field IN_LIST _crosswalk_fields)
      message(FATAL_ERROR
        "Dense-to-sparse '${_crosswalk_dense_operation}' has unknown field "
        "'${_field}'."
      )
    endif()
    if(_field IN_LIST _crosswalk_seen_fields)
      message(FATAL_ERROR
        "Dense-to-sparse '${_crosswalk_dense_operation}' repeats field "
        "'${_field}'."
      )
    endif()
    list(APPEND _crosswalk_seen_fields "${_field}")
    set("_crosswalk_${_field}" "${CMAKE_MATCH_2}")
  endif()
endforeach()
_finalize_coverage_row()
_finalize_crosswalk_row()

if(NOT _dense_count EQUAL _expected_dense_rows)
  message(FATAL_ERROR
    "Dense coverage has ${_dense_count} rows; expected "
    "${_expected_dense_rows}."
  )
endif()
if(NOT _sparse_count EQUAL _expected_sparse_rows)
  message(FATAL_ERROR
    "Sparse coverage has ${_sparse_count} rows; expected "
    "${_expected_sparse_rows}."
  )
endif()
if(NOT _coverage_count EQUAL _expected_total_rows)
  message(FATAL_ERROR
    "BLAS coverage has ${_coverage_count} rows; expected "
    "${_expected_total_rows}."
  )
endif()

foreach(_inventory IN ITEMS dense sparse coverage)
  if(_inventory STREQUAL "dense")
    set(_inventory_keys ${_dense_coverage_keys})
  elseif(_inventory STREQUAL "sparse")
    set(_inventory_keys ${_sparse_coverage_keys})
  else()
    set(_inventory_keys ${_coverage_keys})
  endif()
  list(SORT _inventory_keys)
  string(JOIN "\n" _inventory_contents ${_inventory_keys})
  string(SHA256 _observed_${_inventory}_inventory_sha256
         "${_inventory_contents}")
  if(NOT _observed_${_inventory}_inventory_sha256
     STREQUAL _frozen_${_inventory}_inventory_sha256)
    message(FATAL_ERROR
      "${_inventory} BLAS coverage differs from its frozen identity.\n"
      "  expected: ${_frozen_${_inventory}_inventory_sha256}\n"
      "  observed: ${_observed_${_inventory}_inventory_sha256}"
    )
  endif()
endforeach()

list(REMOVE_DUPLICATES _dense_families)
list(SORT _dense_families)
list(SORT _crosswalk_families)
if(NOT _dense_families STREQUAL _crosswalk_families)
  message(FATAL_ERROR
    "Dense-to-sparse family crosswalk is incomplete.\n"
    "  dense families: ${_dense_families}\n"
    "  crosswalk:      ${_crosswalk_families}"
  )
endif()
list(LENGTH _crosswalk_families _crosswalk_count)
list(SORT _crosswalk_keys)
string(JOIN "\n" _crosswalk_inventory ${_crosswalk_keys})
string(SHA256 _observed_crosswalk_sha256 "${_crosswalk_inventory}")
if(NOT _observed_crosswalk_sha256 STREQUAL _frozen_crosswalk_sha256)
  message(FATAL_ERROR
    "Dense-to-sparse crosswalk differs from its frozen identity.\n"
    "  expected: ${_frozen_crosswalk_sha256}\n"
    "  observed: ${_observed_crosswalk_sha256}"
  )
endif()

set(_report
  "<!-- Generated by cmake/GenerateBlasCoverage.cmake. Do not edit. -->\n\n"
  "<!-- markdownlint-disable MD013 -->\n\n"
  "# BLAS coverage\n\n"
  "Contract status: `${_contract_status}`\n\n"
  "Evidence date: `${_evidence_date}`\n\n"
  "Only `verified` rows count as complete. `implemented` means code exists "
  "but the complete frozen conformance and environment evidence is not yet "
  "recorded.\n\n"
  "## Frozen baselines\n\n"
  "| Contract | Frozen identity |\n"
  "| --- | --- |\n"
  "| dense BLAS/CBLAS | ${_dense_standard}, commit "
  "`${_dense_commit}` |\n"
  "| BLAS quick reference | SHA-256 "
  "`${_dense_quick_reference_sha256}` |\n"
  "| Sparse BLAS | ${_sparse_standard}, Chapter 3 SHA-256 "
  "`${_sparse_chapter_sha256}` |\n"
  "| NIST Sparse BLAS | ${_nist_sparse_version}, SHA-256 "
  "`${_nist_sparse_sha256}` |\n"
  "| CUDA | Toolkit ${_cuda_toolkit}; cuBLAS ${_cublas_version}; "
  "cuSPARSE ${_cusparse_version} |\n\n"
  "## Status summary\n\n"
  "| Rows | Planned | Implemented | Verified | Not applicable | Blocked |\n"
  "| ---: | ---: | ---: | ---: | ---: | ---: |\n"
  "| ${_coverage_count} | ${_planned_count} | ${_implemented_count} | "
  "${_verified_count} | ${_not_applicable_count} | ${_blocked_count} |\n\n"
  "Dense contributes ${_dense_count} rows and Sparse contributes "
  "${_sparse_count}. The dense inventory identity is "
  "`${_observed_dense_inventory_sha256}`, the sparse inventory identity is "
  "`${_observed_sparse_inventory_sha256}`, and the combined identity is "
  "`${_observed_coverage_inventory_sha256}`. The family crosswalk contains "
  "${_crosswalk_count} rows with identity "
  "`${_observed_crosswalk_sha256}`.\n\n"
  "## Routine coverage\n\n"
  "| Module | Level | Operation | Official routine | Scalar type | Layout/"
  "format | Reference CPU | Optimized CPU | CUDA | Status | Public API/"
  "header | Implementation | Tests | Notes |\n"
  "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | "
  "--- | --- | --- |\n"
  "${_coverage_rows_markdown}\n"
  "## Dense-to-sparse family crosswalk\n\n"
  "| Dense family | Sparse analogue | Status | Reason |\n"
  "| --- | --- | --- | --- |\n"
  "${_crosswalk_rows_markdown}\n"
  "<!-- markdownlint-enable MD013 -->\n"
)
string(CONCAT _report ${_report})

get_filename_component(_output_directory "${OUTPUT_REPORT}" DIRECTORY)
file(MAKE_DIRECTORY "${_output_directory}")
file(WRITE "${OUTPUT_REPORT}" "${_report}")

if(DEFINED EXPECTED_REPORT AND NOT "${EXPECTED_REPORT}" STREQUAL "")
  if(NOT EXISTS "${EXPECTED_REPORT}")
    message(FATAL_ERROR
      "Expected generated BLAS report does not exist: ${EXPECTED_REPORT}"
    )
  endif()
  file(READ "${EXPECTED_REPORT}" _expected_report)
  if(NOT _report STREQUAL _expected_report)
    message(FATAL_ERROR
      "Generated BLAS report differs from ${EXPECTED_REPORT}. "
      "Regenerate it from ${BLAS_MANIFEST}."
    )
  endif()
endif()

message(STATUS
  "Validated ${_coverage_count} BLAS rows and ${_crosswalk_count} "
  "dense-to-sparse crosswalk rows."
)
