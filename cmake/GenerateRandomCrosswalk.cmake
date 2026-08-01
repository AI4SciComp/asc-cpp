cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS SOURCE_DIR MANIFEST OUTPUT)
  if(NOT DEFINED "${_required}" OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required.")
  endif()
endforeach()

function(_extract_quoted key output)
  if(NOT _manifest MATCHES "(^|\n)${key}: \"([^\"]*)\"")
    message(FATAL_ERROR "Random crosswalk omits ${key}.")
  endif()
  set("${output}" "${CMAKE_MATCH_2}" PARENT_SCOPE)
endfunction()

function(_extract_integer key output)
  if(NOT _manifest MATCHES "(^|\n)${key}: ([0-9]+)")
    message(FATAL_ERROR "Random crosswalk omits integer ${key}.")
  endif()
  set("${output}" "${CMAKE_MATCH_2}" PARENT_SCOPE)
endfunction()

function(_require_sha256 label value)
  string(LENGTH "${value}" _sha256_length)
  if(NOT _sha256_length EQUAL 64 OR NOT "${value}" MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "${label} is not a lowercase SHA-256: ${value}")
  endif()
endfunction()

function(_markdown value output)
  set(_value "${value}")
  string(REPLACE "|" "\\|" _value "${_value}")
  if(_value STREQUAL "")
    set(_value "-")
  endif()
  set("${output}" "${_value}" PARENT_SCOPE)
endfunction()

file(READ "${MANIFEST}" _manifest)
string(REPLACE "\r\n" "\n" _manifest "${_manifest}")
string(REPLACE "\r" "\n" _manifest "${_manifest}")
string(REPLACE ";" "\\;" _manifest_lines "${_manifest}")
string(REPLACE "\n" ";" _manifest_lines "${_manifest_lines}")

set(_expected_metadata_fields
  schema_version
  contract_status
  evidence_date
  source_repository
  source_commit
  source_license
  source_license_sha256
  destination_repository
  destination_base_commit
  destination_license
  allowed_classifications
  expected_source_artifacts
  expected_crosswalk_rows
  expected_clean_room_required
  expected_incomplete
  expected_permission_relicensing_required
  expected_rejected
  source_inventory_sha256
  crosswalk_sha256
  provenance_identity_sha256
  splitmix64_url
  splitmix64_sha256
  xoroshiro64star_url
  xoroshiro64star_sha256
  xoroshiro128plus_url
  xoroshiro128plus_sha256
  pcg32_url
  pcg32_sha256
  sobol_data_url
  sobol_data_sha256
  sobol_license_url
  sobol_license_sha256
  sobol_upstream_program_url
  sobol_upstream_program_sha256
  burkardt_comparison_url
  burkardt_comparison_sha256
)
set(_metadata_fields)
foreach(_line IN LISTS _manifest_lines)
  if(_line STREQUAL "crosswalk:")
    break()
  elseif(_line STREQUAL "")
    continue()
  elseif(_line MATCHES "^([a-z0-9_]+): (\"[^\"]*\"|[0-9]+)$")
    set(_metadata_field "${CMAKE_MATCH_1}")
    if(_metadata_field IN_LIST _metadata_fields)
      message(FATAL_ERROR
        "Random crosswalk repeats metadata field ${_metadata_field}."
      )
    endif()
    list(APPEND _metadata_fields "${_metadata_field}")
  else()
    message(FATAL_ERROR
      "Random crosswalk has malformed top-level metadata: ${_line}"
    )
  endif()
endforeach()
list(SORT _expected_metadata_fields)
list(SORT _metadata_fields)
if(NOT _metadata_fields STREQUAL _expected_metadata_fields)
  message(FATAL_ERROR
    "Random crosswalk metadata fields differ from the frozen schema.\n"
    "  seen: ${_metadata_fields}\n"
    "  required: ${_expected_metadata_fields}"
  )
endif()

_extract_integer("schema_version" _schema_version)
_extract_quoted("contract_status" _contract_status)
_extract_quoted("evidence_date" _evidence_date)
_extract_quoted("source_repository" _source_repository)
_extract_quoted("source_commit" _source_commit)
_extract_quoted("source_license" _source_license)
_extract_quoted("source_license_sha256" _source_license_sha256)
_extract_quoted("destination_repository" _destination_repository)
_extract_quoted("destination_base_commit" _destination_base_commit)
_extract_quoted("destination_license" _destination_license)
_extract_quoted("allowed_classifications" _allowed_classifications)
_extract_integer("expected_source_artifacts" _expected_source_artifacts)
_extract_integer("expected_crosswalk_rows" _expected_crosswalk_rows)
_extract_integer("expected_clean_room_required"
                 _expected_clean_room_required)
_extract_integer("expected_incomplete" _expected_incomplete)
_extract_integer("expected_permission_relicensing_required"
                 _expected_permission_relicensing_required)
_extract_integer("expected_rejected" _expected_rejected)
_extract_quoted("source_inventory_sha256" _source_inventory_sha256)
_extract_quoted("crosswalk_sha256" _expected_crosswalk_sha256)
_extract_quoted("provenance_identity_sha256"
                _expected_provenance_identity_sha256)

set(_upstream_names
  splitmix64
  xoroshiro64star
  xoroshiro128plus
  pcg32
  sobol_data
  sobol_license
  sobol_upstream_program
  burkardt_comparison
)
foreach(_upstream IN LISTS _upstream_names)
  _extract_quoted("${_upstream}_url" "_${_upstream}_url")
  _extract_quoted("${_upstream}_sha256" "_${_upstream}_sha256")
  _require_sha256("${_upstream}_sha256" "${_${_upstream}_sha256}")
endforeach()

set(_frozen_contract_status
  "issue-12-random-architecture-feature-gate-b-candidate"
)
set(_frozen_source_commit
  "f6294e9079262682ce63ae7ff2d8a643e658bf5d"
)
set(_frozen_destination_base_commit
  "bd73a5bc63524bfec8fae9b4d6446e0fcdf8e851"
)
set(_frozen_allowed_classifications
  "equivalent,incomplete,absent,rejected,clean-room-required,permission-relicensing-required"
)
set(_frozen_source_inventory_sha256
  "f47d8ecf5cfbb689fe0643d1bbb8e772138523b68a8055cdb73dc4ae3b1cb6ff"
)
set(_frozen_crosswalk_sha256
  "855b08c06db84b81e090d64e2a5817a54f48ef829978aa385b40e6c47a2e1926"
)
set(_frozen_provenance_identity_sha256
  "4ddc61de0313dbea28bdf74c352a64e7b0de25b792b4a88f9ab21af78926d492"
)

foreach(_frozen IN ITEMS
    contract_status
    source_commit
    destination_base_commit
    allowed_classifications
    source_inventory_sha256
)
  set(_declared_variable "_${_frozen}")
  set(_frozen_variable "_frozen_${_frozen}")
  if(NOT "${${_declared_variable}}" STREQUAL
         "${${_frozen_variable}}")
    message(FATAL_ERROR
      "Random contract ${_frozen} differs from the frozen Issue 12 value."
    )
  endif()
endforeach()
if(NOT _expected_crosswalk_sha256 STREQUAL _frozen_crosswalk_sha256)
  message(FATAL_ERROR
    "Random crosswalk declared identity differs from the frozen Issue 12 "
    "value."
  )
endif()
if(NOT _expected_provenance_identity_sha256 STREQUAL
       _frozen_provenance_identity_sha256)
  message(FATAL_ERROR
    "Random provenance metadata identity differs from the frozen Issue 12 "
    "value."
  )
endif()
if(NOT _schema_version EQUAL 1)
  message(FATAL_ERROR "Unsupported random crosswalk schema version.")
endif()
if(NOT _source_license MATCHES "GPL-3.0"
   OR NOT _destination_license STREQUAL "Apache-2.0")
  message(FATAL_ERROR "Random crosswalk license boundary changed.")
endif()
_require_sha256("source_license_sha256" "${_source_license_sha256}")
_require_sha256("source_inventory_sha256" "${_source_inventory_sha256}")
_require_sha256("crosswalk_sha256" "${_expected_crosswalk_sha256}")
_require_sha256("provenance_identity_sha256"
                "${_expected_provenance_identity_sha256}")

set(_row_fields
  id
  source_component
  source_path
  source_sha256
  category
  classification
  current_api
  current_path
  destination_owner
  child_issue
  provenance_route
  planned_files
  verification
  decision
)
set(_row_active FALSE)
set(_row_seen)
set(_row_ids)
set(_source_records)
set(_identity_rows)
set(_report_rows)
set(_row_count 0)
set(_clean_room_required_count 0)
set(_incomplete_count 0)
set(_permission_relicensing_required_count 0)
set(_rejected_count 0)
set(_equivalent_count 0)
set(_absent_count 0)

macro(_finalize_random_row)
  if(_row_active)
    set(_seen "${_row_seen}")
    list(SORT _seen)
    set(_required_fields "${_row_fields}")
    list(SORT _required_fields)
    if(NOT _seen STREQUAL _required_fields)
      message(FATAL_ERROR
        "Random row ${_row_id} fields differ from the frozen schema.\n"
        "  seen: ${_seen}\n"
        "  required: ${_required_fields}"
      )
    endif()
    if(_row_id IN_LIST _row_ids)
      message(FATAL_ERROR "Duplicate random crosswalk id: ${_row_id}")
    endif()
    list(APPEND _row_ids "${_row_id}")
    if(NOT _row_id MATCHES "^RND-[0-9][0-9][0-9]$")
      message(FATAL_ERROR "Invalid random crosswalk id: ${_row_id}")
    endif()
    string(REPLACE "," ";" _allowed "${_allowed_classifications}")
    if(NOT _row_classification IN_LIST _allowed)
      message(FATAL_ERROR
        "Random row ${_row_id} has unknown classification: "
        "${_row_classification}"
      )
    endif()
    if(_row_source_path STREQUAL "none")
      if(NOT _row_source_sha256 STREQUAL "not-applicable")
        message(FATAL_ERROR
          "Random row ${_row_id} has no source but claims a source hash."
        )
      endif()
    else()
      _require_sha256("Random row ${_row_id} source_sha256"
                     "${_row_source_sha256}")
      if(NOT _row_source_path MATCHES
         "^(random/|tests/random/|benchmarks/).+"
         OR _row_source_path MATCHES ";")
        message(FATAL_ERROR
          "Random row ${_row_id} has an invalid source artifact path: "
          "${_row_source_path}"
        )
      endif()
      list(APPEND _source_records
        "${_row_source_path}|${_row_source_sha256}"
      )
    endif()
    if(_row_current_path)
      if(NOT EXISTS "${SOURCE_DIR}/${_row_current_path}")
        message(FATAL_ERROR
          "Random row ${_row_id} current path does not exist: "
          "${_row_current_path}"
        )
      endif()
      if(NOT _row_current_path MATCHES "^(include/asc|src)/.+")
        message(FATAL_ERROR
          "Random row ${_row_id} current path is outside product roots."
        )
      endif()
    endif()
    if(NOT _row_child_issue MATCHES "(^|,| and )(13|14|15)(,| and |$)")
      message(FATAL_ERROR
        "Random row ${_row_id} is not assigned to child 13, 14, or 15."
      )
    endif()
    if(_row_planned_files MATCHES "MdeCpp|mdecpp|sobol\\.(txt|bin)")
      message(FATAL_ERROR
        "Random row ${_row_id} plans a forbidden MdeCpp artifact."
      )
    endif()
    if(_row_decision STREQUAL "" OR _row_verification STREQUAL "")
      message(FATAL_ERROR
        "Random row ${_row_id} lacks a decision or verification plan."
      )
    endif()
    if(_row_classification STREQUAL "clean-room-required")
      math(EXPR _clean_room_required_count
        "${_clean_room_required_count} + 1"
      )
    elseif(_row_classification STREQUAL "incomplete")
      math(EXPR _incomplete_count "${_incomplete_count} + 1")
    elseif(_row_classification STREQUAL
           "permission-relicensing-required")
      math(EXPR _permission_relicensing_required_count
        "${_permission_relicensing_required_count} + 1"
      )
    elseif(_row_classification STREQUAL "rejected")
      math(EXPR _rejected_count "${_rejected_count} + 1")
    elseif(_row_classification STREQUAL "equivalent")
      math(EXPR _equivalent_count "${_equivalent_count} + 1")
    elseif(_row_classification STREQUAL "absent")
      math(EXPR _absent_count "${_absent_count} + 1")
    endif()
    math(EXPR _row_count "${_row_count} + 1")

    set(_identity "")
    foreach(_identity_field IN LISTS _row_fields)
      set(_identity_value "${_row_${_identity_field}}")
      string(LENGTH "${_identity_value}" _identity_value_length)
      string(APPEND _identity
        "${_identity_field}:${_identity_value_length}:${_identity_value}"
      )
    endforeach()
    string(SHA256 _row_identity_sha256 "${_identity}")
    list(APPEND _identity_rows "${_row_id}|${_row_identity_sha256}")

    _markdown("${_row_source_component}" _md_source_component)
    _markdown("${_row_source_path}" _md_source_path)
    _markdown("${_row_category}" _md_category)
    _markdown("${_row_classification}" _md_classification)
    _markdown("${_row_current_api}" _md_current_api)
    _markdown("${_row_destination_owner}" _md_owner)
    _markdown("${_row_child_issue}" _md_child)
    _markdown("${_row_provenance_route}" _md_provenance)
    _markdown("${_row_planned_files}" _md_planned_files)
    string(REPLACE ";" "<br>" _md_planned_files "${_md_planned_files}")
    _markdown("${_row_verification}" _md_verification)
    _markdown("${_row_decision}" _md_decision)
    if(_row_current_path)
      set(_md_current
        "`${_md_current_api}`<br>[`${_row_current_path}`](../${_row_current_path})"
      )
    else()
      set(_md_current "`${_md_current_api}`")
    endif()
    string(APPEND _report_rows
      "| `${_row_id}` | `${_md_source_component}`<br>`${_md_source_path}` | "
      "${_md_category} | **${_md_classification}** | ${_md_current} | "
      "`${_md_owner}` | #${_md_child} | ${_md_provenance} | "
      "${_md_planned_files} | ${_md_verification} | ${_md_decision} |\n"
    )
  endif()
endmacro()

set(_section "")
set(_crosswalk_section_count 0)
foreach(_line IN LISTS _manifest_lines)
  if(_line STREQUAL "crosswalk:")
    math(EXPR _crosswalk_section_count "${_crosswalk_section_count} + 1")
    if(NOT _crosswalk_section_count EQUAL 1)
      message(FATAL_ERROR
        "Random crosswalk repeats the crosswalk section."
      )
    endif()
    set(_section "crosswalk")
  elseif(_section STREQUAL "crosswalk"
         AND _line MATCHES "^  - id: \"([^\"]+)\"$")
    set(_next_id "${CMAKE_MATCH_1}")
    _finalize_random_row()
    set(_row_active TRUE)
    set(_row_seen id)
    set(_row_id "${_next_id}")
  elseif(_section STREQUAL "crosswalk"
         AND _line MATCHES "^    ([a-z0-9_]+): \"([^\"]*)\"$")
    set(_field "${CMAKE_MATCH_1}")
    set(_value "${CMAKE_MATCH_2}")
    if(NOT _field IN_LIST _row_fields)
      message(FATAL_ERROR
        "Random row ${_row_id} contains unknown field ${_field}."
      )
    endif()
    if(_field IN_LIST _row_seen)
      message(FATAL_ERROR
        "Random row ${_row_id} repeats field ${_field}."
      )
    endif()
    list(APPEND _row_seen "${_field}")
    set("_row_${_field}" "${_value}")
  elseif(_section STREQUAL "crosswalk" AND NOT _line STREQUAL "")
    message(FATAL_ERROR
      "Random crosswalk contains a malformed row line: ${_line}"
    )
  endif()
endforeach()
_finalize_random_row()

if(NOT _crosswalk_section_count EQUAL 1)
  message(FATAL_ERROR "Random crosswalk omits its crosswalk section.")
endif()

if(NOT _row_count EQUAL _expected_crosswalk_rows)
  message(FATAL_ERROR
    "Random crosswalk has ${_row_count} rows; expected "
    "${_expected_crosswalk_rows}."
  )
endif()
foreach(_count IN ITEMS
    clean_room_required
    incomplete
    permission_relicensing_required
    rejected
)
  if(NOT _${_count}_count EQUAL _expected_${_count})
    message(FATAL_ERROR
      "Random ${_count} count is ${_${_count}_count}; expected "
      "${_expected_${_count}}."
    )
  endif()
endforeach()
if(NOT _equivalent_count EQUAL 0 OR NOT _absent_count EQUAL 0)
  message(FATAL_ERROR
    "Issue 12 freezes zero equivalent and zero bare-absent rows."
  )
endif()

list(REMOVE_DUPLICATES _source_records)
list(SORT _source_records)
list(LENGTH _source_records _source_artifact_count)
if(NOT _source_artifact_count EQUAL _expected_source_artifacts)
  message(FATAL_ERROR
    "Random source inventory has ${_source_artifact_count} artifacts; "
    "expected ${_expected_source_artifacts}."
  )
endif()
string(JOIN "\n" _source_identity ${_source_records})
string(SHA256 _observed_source_inventory_sha256 "${_source_identity}")
if(NOT _observed_source_inventory_sha256 STREQUAL
       _frozen_source_inventory_sha256)
  message(FATAL_ERROR
    "Random source inventory identity differs from its frozen Issue 12 "
    "identity.\n"
    "  expected: ${_frozen_source_inventory_sha256}\n"
    "  observed: ${_observed_source_inventory_sha256}"
  )
endif()

list(SORT _identity_rows)
string(JOIN "\n" _identity ${_identity_rows})
string(SHA256 _observed_crosswalk_sha256 "${_identity}")
if(NOT _observed_crosswalk_sha256 STREQUAL _frozen_crosswalk_sha256)
  message(FATAL_ERROR
    "Random crosswalk identity differs from its frozen Issue 12 identity.\n"
    "  expected: ${_frozen_crosswalk_sha256}\n"
    "  observed: ${_observed_crosswalk_sha256}"
  )
endif()

set(_provenance_records
  "schema_version|${_schema_version}"
  "contract_status|${_contract_status}"
  "evidence_date|${_evidence_date}"
  "source_repository|${_source_repository}"
  "source_commit|${_source_commit}"
  "source_license|${_source_license}"
  "source_license_sha256|${_source_license_sha256}"
  "destination_repository|${_destination_repository}"
  "destination_base_commit|${_destination_base_commit}"
  "destination_license|${_destination_license}"
  "allowed_classifications|${_allowed_classifications}"
  "expected_source_artifacts|${_expected_source_artifacts}"
  "expected_crosswalk_rows|${_expected_crosswalk_rows}"
  "expected_clean_room_required|${_expected_clean_room_required}"
  "expected_incomplete|${_expected_incomplete}"
  "expected_permission_relicensing_required|${_expected_permission_relicensing_required}"
  "expected_rejected|${_expected_rejected}"
  "source_inventory_sha256|${_source_inventory_sha256}"
  "crosswalk_sha256|${_expected_crosswalk_sha256}"
)
foreach(_upstream IN LISTS _upstream_names)
  list(APPEND _provenance_records
    "upstream|${_upstream}|${_${_upstream}_url}|${_${_upstream}_sha256}"
  )
endforeach()
list(SORT _provenance_records)
string(JOIN "\n" _provenance_identity ${_provenance_records})
string(SHA256 _observed_provenance_identity_sha256
       "${_provenance_identity}")
if(NOT _observed_provenance_identity_sha256 STREQUAL
       _frozen_provenance_identity_sha256)
  message(FATAL_ERROR
    "Random provenance metadata identity differs from its frozen Issue 12 "
    "identity.\n"
    "  expected: ${_frozen_provenance_identity_sha256}\n"
    "  observed: ${_observed_provenance_identity_sha256}"
  )
endif()

set(_upstream_rows "")
foreach(_upstream IN LISTS _upstream_names)
  string(REPLACE "_" " " _upstream_label "${_upstream}")
  string(APPEND _upstream_rows
    "| ${_upstream_label} | [source](${_${_upstream}_url}) | "
    "`${_${_upstream}_sha256}` |\n"
  )
endforeach()

string(CONCAT _report
  "<!-- markdownlint-disable MD013 -->\n"
  "<!-- Generated from random-crosswalk.yaml. Do not edit directly. -->\n\n"
  "# Random architecture and provenance crosswalk\n\n"
  "Contract status: `${_contract_status}`\n\n"
  "Evidence date: `${_evidence_date}`\n\n"
  "The frozen MdeCpp source is commit `${_source_commit}` under the "
  "repository GPL-3.0-only working assumption. The destination is "
  "Apache-2.0. The 20-artifact source inventory identity is "
  "`${_source_inventory_sha256}`, the 33-row decision identity is "
  "`${_observed_crosswalk_sha256}`, and the frozen evidence-metadata "
  "identity is `${_observed_provenance_identity_sha256}`. No MdeCpp "
  "implementation, test, "
  "benchmark, generated data, or prose is approved for copying.\n\n"
  "## Disposition summary\n\n"
  "| Clean-room required | Incomplete ASC behavior | Permission route | "
  "Rejected | Equivalent | Bare absent |\n"
  "| ---: | ---: | ---: | ---: | ---: | ---: |\n"
  "| ${_clean_room_required_count} | ${_incomplete_count} | "
  "${_permission_relicensing_required_count} | ${_rejected_count} | "
  "${_equivalent_count} | ${_absent_count} |\n\n"
  "## Approved upstream identities\n\n"
  "| Artifact | Authoritative source | SHA-256 |\n"
  "| --- | --- | --- |\n"
  "${_upstream_rows}\n"
  "Hashes pin the material inspected on the evidence date. They do not "
  "authorize MdeCpp reuse. Only the row-specific clean-room or compatible "
  "permission route is approved.\n\n"
  "## Component crosswalk\n\n"
  "| ID | MdeCpp component and path | Category | Classification | "
  "Current ASC evidence | Owner | Child | Provenance route | Planned files | "
  "Verification | Target ASC API / decision |\n"
  "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |\n"
  "${_report_rows}\n"
  "<!-- markdownlint-enable MD013 -->\n"
)

get_filename_component(_output_directory "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_output_directory}")
file(WRITE "${OUTPUT}" "${_report}")

if(DEFINED VERIFY_OUTPUT AND NOT "${VERIFY_OUTPUT}" STREQUAL "")
  if(NOT EXISTS "${VERIFY_OUTPUT}")
    message(FATAL_ERROR "Random crosswalk report is missing: ${VERIFY_OUTPUT}")
  endif()
  file(READ "${VERIFY_OUTPUT}" _checked_report)
  if(NOT _checked_report STREQUAL _report)
    message(FATAL_ERROR
      "Generated Random crosswalk differs from ${VERIFY_OUTPUT}."
    )
  endif()
endif()

message(STATUS
  "Validated ${_row_count} Random crosswalk decisions and generated ${OUTPUT}"
)
