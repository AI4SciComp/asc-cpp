cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED ASC_CPP_SOURCE_DIR OR "${ASC_CPP_SOURCE_DIR}" STREQUAL "")
  message(FATAL_ERROR "ASC_CPP_SOURCE_DIR is required.")
endif()
cmake_path(
  ABSOLUTE_PATH ASC_CPP_SOURCE_DIR
  NORMALIZE
  OUTPUT_VARIABLE ASC_CPP_SOURCE_DIR
)

set(_required_files
  SECURITY.md
  CITATION.cff
  docs/README.md
  docs/installation.md
  docs/release-process.md
  docs/support-matrix.md
  docs/contracts/capability-manifest.yaml
  docs/contracts/dependency-manifest.yaml
  release/release-plan.md
  release/release-notes-v0.9.0.md
  release/known-limitations-v0.9.0.md
  release/release-tree-manifest.md
)
foreach(_relative_path IN LISTS _required_files)
  if(NOT EXISTS "${ASC_CPP_SOURCE_DIR}/${_relative_path}")
    message(FATAL_ERROR "Required release document is missing: ${_relative_path}")
  endif()
endforeach()

function(_require_text relative_path expected)
  file(READ "${ASC_CPP_SOURCE_DIR}/${relative_path}" _contents)
  string(FIND "${_contents}" "${expected}" _position)
  if(_position EQUAL -1)
    message(FATAL_ERROR "${relative_path} omits required text: ${expected}")
  endif()
endfunction()

_require_text(SECURITY.md "| `0.9.x` | Supported after public 0.9.0 publication |")
_require_text(SECURITY.md "private vulnerability-reporting")
_require_text(SECURITY.md "route is enabled and verified")
_require_text(docs/support-matrix.md "provider-free C++20 reference release")
_require_text(docs/support-matrix.md "experimental")
_require_text(docs/installation.md "8a7dcbad3a97267cce59810aff24de800a3497a7")
_require_text(docs/installation.md "73299eca4b80b8a5571622636fe12c88f67602eba4e99f24368d5405b9bc0021")
_require_text(docs/contracts/capability-manifest.yaml
              "status: \"v0.9.0-release-contract\"")
_require_text(docs/contracts/dependency-manifest.yaml
              "status: \"v0.9.0-release-contract\"")
_require_text(release/release-plan.md
              "The release may become public only after all of the following are verified:")

if(EXISTS "${ASC_CPP_SOURCE_DIR}/docs/development")
  file(GLOB_RECURSE _development_files LIST_DIRECTORIES FALSE
       "${ASC_CPP_SOURCE_DIR}/docs/development/*")
  if(_development_files)
    message(FATAL_ERROR
      "Temporary docs/development material remains in the release tree: "
      "${_development_files}"
    )
  endif()
endif()

set(_removed_release_documents
  docs/api.md
  docs/blas-completion-audit.md
  docs/random-completion-audit.md
  docs/architecture/disagreement-matrix.md
  docs/architecture/implementation-plan.md
  docs/architecture/internal-analysis.md
  docs/architecture/independent-analysis.md
  docs/architecture/release-roadmap.md
)
foreach(_relative_path IN LISTS _removed_release_documents)
  if(EXISTS "${ASC_CPP_SOURCE_DIR}/${_relative_path}")
    message(FATAL_ERROR "Archive-only document was restored: ${_relative_path}")
  endif()
endforeach()

file(GLOB_RECURSE _release_documents LIST_DIRECTORIES FALSE
  "${ASC_CPP_SOURCE_DIR}/docs/*.md"
  "${ASC_CPP_SOURCE_DIR}/docs/*.dox"
  "${ASC_CPP_SOURCE_DIR}/release/*.md"
)
list(REMOVE_ITEM _release_documents
  "${ASC_CPP_SOURCE_DIR}/release/release-tree-manifest.md"
)
foreach(_path IN LISTS _release_documents)
  file(READ "${_path}" _contents)
  foreach(_stale_pattern IN ITEMS
      "[Mm]ilestone [0-9]+"
      "[Ii]ssue [0-9]+"
      "[Ff]eature [Gg]ate"
      "Status:[^\n]*(candidate|unreleased)"
      "docs/development/")
    if(_contents MATCHES "${_stale_pattern}")
      file(RELATIVE_PATH _relative "${ASC_CPP_SOURCE_DIR}" "${_path}")
      message(FATAL_ERROR
        "Release-facing document ${_relative} contains stale wording matching "
        "${_stale_pattern}."
      )
    endif()
  endforeach()
endforeach()

message(STATUS
  "Release documents, support boundary, immutable dependency identities, "
  "and archive-only cleanup are consistent."
)
