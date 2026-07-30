cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED ASC_CPP_SOURCE_DIR OR "${ASC_CPP_SOURCE_DIR}" STREQUAL "")
  message(FATAL_ERROR "ASC_CPP_SOURCE_DIR is required.")
endif()

set(_security_file "${ASC_CPP_SOURCE_DIR}/SECURITY.md")
set(_capability_file
  "${ASC_CPP_SOURCE_DIR}/docs/development/asc-cpp-architecture/capability-manifest.yaml"
)
set(_roadmap_file
  "${ASC_CPP_SOURCE_DIR}/docs/development/asc-cpp-architecture/release-roadmap.md"
)
set(_documentation_index_file "${ASC_CPP_SOURCE_DIR}/docs/README.md")
foreach(_required_file IN ITEMS
    "${_security_file}"
    "${_capability_file}"
    "${_roadmap_file}"
    "${_documentation_index_file}")
  if(NOT EXISTS "${_required_file}")
    message(FATAL_ERROR "Required policy input does not exist: ${_required_file}")
  endif()
endforeach()

file(READ "${_security_file}" _security)
file(READ "${_capability_file}" _capability)
file(READ "${_roadmap_file}" _roadmap)
file(READ "${_documentation_index_file}" _documentation_index)

foreach(_security_text IN ITEMS
    "unreleased `0.9.0` Milestone 8 correction candidate"
    "Core, Utilities, Expression, Dense, Sparse, and Random"
    "Core, Dense, Sparse, and Random CUDA facets"
    "Dense and Sparse Random storage facets"
    "CMake, test, hardening, ABI, benchmark, and CI tooling"
    "unsafe path construction, traversal, symlink escape, recursive deletion"
    "misleading required/optional component availability"
    "CUDA facets are explicit optional providers")
  string(FIND "${_security}" "${_security_text}" _security_position)
  if(_security_position EQUAL -1)
    message(FATAL_ERROR
      "SECURITY.md is missing current Milestone 8 boundary text: "
      "${_security_text}"
    )
  endif()
endforeach()

foreach(_stale_security_text IN ITEMS
    "Milestone 1 Core candidate"
    "All numerical containers, kernels, optimized providers, and GPU backends "
    "are outside")
  string(FIND "${_security}" "${_stale_security_text}"
    _stale_security_position
  )
  if(NOT _stale_security_position EQUAL -1)
    message(FATAL_ERROR
      "SECURITY.md contains stale scope text: ${_stale_security_text}"
    )
  endif()
endforeach()

string(FIND "${_capability}"
  "status: \"milestone-8-publication-checkpoint-b\""
  _capability_status_position
)
if(_capability_status_position EQUAL -1)
  message(FATAL_ERROR
    "The capability manifest is not at Milestone 8 Publication Checkpoint B."
  )
endif()
string(FIND "${_roadmap}"
  "Milestone 8 reached local Publication"
  _roadmap_status_position
)
if(_roadmap_status_position EQUAL -1)
  message(FATAL_ERROR
    "The release roadmap is not at Milestone 8 Publication Checkpoint B."
  )
endif()

string(FIND "${_documentation_index}"
  "## Retained historical documents"
  _retained_historical_documents_position
)
if(NOT _retained_historical_documents_position EQUAL -1)
  message(FATAL_ERROR
    "The documentation index still claims deleted historical documents are "
    "retained."
  )
endif()

set(_removed_historical_documents
  docs/architecture.md
  docs/build-system.md
  docs/design/architecture_blueprint_v1.md
  docs/design/architecture_review_v1.md
  docs/design/array_design.md
  docs/design/core_design.md
  docs/design/linalg_design.md
  docs/design/random_design.md
  docs/design/utilities_design.md
  docs/migration/array.md
  docs/migration/core.md
  docs/migration/handoff.md
  docs/migration/inventory.md
  docs/migration/linalg.md
  docs/migration/random.md
  docs/migration/utilities.md
  docs/modules/array.md
  docs/modules/linalg.md
  docs/optional-backends.md
  docs/testing.md
)
foreach(_removed_document IN LISTS _removed_historical_documents)
  if(EXISTS "${ASC_CPP_SOURCE_DIR}/${_removed_document}")
    message(FATAL_ERROR
      "Deleted historical document was restored: ${_removed_document}"
    )
  endif()
endforeach()

file(GLOB_RECURSE _documentation_files
  LIST_DIRECTORIES FALSE
  "${ASC_CPP_SOURCE_DIR}/docs/*.md"
)
set(_historical_banner_count 0)
foreach(_documentation_file IN LISTS _documentation_files)
  file(READ "${_documentation_file}" _documentation)
  string(FIND "${_documentation}"
    "**Superseded historical document.**"
    _historical_banner_position
  )
  if(NOT _historical_banner_position EQUAL -1)
    math(EXPR _historical_banner_count "${_historical_banner_count} + 1")
  endif()
endforeach()

if(NOT _historical_banner_count EQUAL 0)
  message(FATAL_ERROR
    "Superseded historical documents must not be restored; found "
    "${_historical_banner_count} historical banner(s)."
  )
endif()

message(STATUS
  "Milestone 8 security policy, 20 removed historical documents, and zero "
  "historical banners are consistent."
)
