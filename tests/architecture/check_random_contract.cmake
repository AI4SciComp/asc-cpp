cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS SOURCE_DIR BINARY_DIR)
  if(NOT DEFINED "${_required}" OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required.")
  endif()
endforeach()

set(_manifest
  "${SOURCE_DIR}/docs/development/asc-cpp-architecture/random-crosswalk.yaml"
)
set(_report "${SOURCE_DIR}/docs/random-crosswalk.md")
set(_generator "${SOURCE_DIR}/cmake/GenerateRandomCrosswalk.cmake")
set(_generated "${BINARY_DIR}/random-crosswalk.generated.md")

execute_process(
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSOURCE_DIR:PATH=${SOURCE_DIR}"
    "-DMANIFEST:FILEPATH=${_manifest}"
    "-DOUTPUT:FILEPATH=${_generated}"
    "-DVERIFY_OUTPUT:FILEPATH=${_report}"
    -P "${_generator}"
  RESULT_VARIABLE _generate_result
  OUTPUT_VARIABLE _generate_stdout
  ERROR_VARIABLE _generate_stderr
)
if(NOT _generate_result EQUAL 0)
  message(FATAL_ERROR
    "Random crosswalk validation failed.\n"
    "stdout:\n${_generate_stdout}\n"
    "stderr:\n${_generate_stderr}"
  )
endif()

file(READ "${_manifest}" _valid_manifest)
if(_valid_manifest MATCHES "TO_BE_FROZEN|<issue|<exact|<approved|<non-goal")
  message(FATAL_ERROR "Random contract contains an unresolved placeholder.")
endif()

function(_require_document_text relative_path expected)
  set(_path "${SOURCE_DIR}/${relative_path}")
  if(NOT EXISTS "${_path}")
    message(FATAL_ERROR "Required Random contract document is missing: ${_path}")
  endif()
  file(READ "${_path}" _contents)
  string(FIND "${_contents}" "${expected}" _position)
  if(_position EQUAL -1)
    message(FATAL_ERROR
      "Random contract document ${relative_path} omits: ${expected}"
    )
  endif()
endfunction()

_require_document_text(
  "README.md"
  "Random completion audit"
)
_require_document_text(
  "docs/README.md"
  "Random crosswalk"
)
_require_document_text(
  "docs/modules/random.md"
  "FillDenseMultivariateNormal"
)
_require_document_text(
  "docs/examples/random-qmc.md"
  "GenerateLatinHypercubeJittered"
)
_require_document_text(
  "docs/examples/random-advanced.md"
  "GenerateSparseStructure"
)
_require_document_text(
  "docs/random-completion-audit.md"
  "Required branch: `test/16-random-completion`"
)
_require_document_text(
  "docs/random-crosswalk.md"
  "Retain Uniform01; add leading-bit canonical engine extraction"
)
_require_document_text(
  "docs/development/asc-cpp-architecture/decisions/0020-random-contract.md"
  "Required branch: `feature/15-random-samplers`."
)
_require_document_text(
  "docs/development/asc-cpp-architecture/decisions/0020-random-contract.md"
  "Non-goals are a sampler hierarchy, new storage protocol"
)
_require_document_text(
  "docs/development/asc-cpp-architecture/mdecpp-disposition.yaml"
  "Issue 13 implements explicit nondeterministic acquisition"
)

function(_expect_failure name manifest_text expected)
  set(_fixture "${BINARY_DIR}/random-contract-${name}.yaml")
  set(_output "${BINARY_DIR}/random-contract-${name}.md")
  file(WRITE "${_fixture}" "${manifest_text}")
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}"
      "-DSOURCE_DIR:PATH=${SOURCE_DIR}"
      "-DMANIFEST:FILEPATH=${_fixture}"
      "-DOUTPUT:FILEPATH=${_output}"
      -P "${_generator}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
  )
  if(_result EQUAL 0)
    message(FATAL_ERROR
      "Invalid Random contract fixture '${name}' unexpectedly passed."
    )
  endif()
  string(CONCAT _diagnostic "${_stdout}" "${_stderr}")
  if(NOT _diagnostic MATCHES "${expected}")
    message(FATAL_ERROR
      "Invalid Random contract fixture '${name}' returned the wrong error.\n"
      "expected: ${expected}\n"
      "observed:\n${_diagnostic}"
    )
  endif()
endfunction()

string(REPLACE
  "source_commit: \"f6294e9079262682ce63ae7ff2d8a643e658bf5d\""
  "source_commit: \"0000000000000000000000000000000000000000\""
  _changed_source
  "${_valid_manifest}"
)
_expect_failure(changed-source "${_changed_source}"
                "source_commit differs from the approved Issue 16 value")

string(REPLACE
  "asc::SplitMix64"
  "asc::MissingSplitMix64"
  _changed_api
  "${_valid_manifest}"
)
_expect_failure(changed-api "${_changed_api}"
                "public API is absent")

string(REPLACE
  "tests/random/seed_test.cc"
  "tests/random/missing_seed_test.cc"
  _changed_evidence_path
  "${_valid_manifest}"
)
_expect_failure(changed-evidence-path "${_changed_evidence_path}"
                "evidence path does not exist")

string(REPLACE
  "efb0ddedcb6a11ed3defd85764e352f16b09f13784c15225a1b2dd19dcbc2d6b"
  "0000000000000000000000000000000000000000000000000000000000000000"
  _changed_artifact
  "${_valid_manifest}"
)
_expect_failure(changed-artifact "${_changed_artifact}"
                "source inventory identity")

string(REPLACE
  "classification: \"equivalent\""
  "classification: \"incomplete\""
  _changed_classification
  "${_valid_manifest}"
)
_expect_failure(changed-classification "${_changed_classification}"
                "equivalent count")

string(REPLACE
  "splitmix64_sha256: \"071795a8e29978a5cbd7015ce8f7d772e7ab4631e574e9102b748fe99105ff3d\""
  "splitmix64_sha256: \"0000000000000000000000000000000000000000000000000000000000000000\""
  _changed_upstream
  "${_valid_manifest}"
)
_expect_failure(changed-upstream "${_changed_upstream}"
                "provenance metadata identity")

string(REPLACE
  "Reject second-resolution time seeding because it is collision-prone and not an entropy source."
  "Reject every implicit time-seeding path."
  _changed_decision
  "${_valid_manifest}"
)
_expect_failure(changed-decision "${_changed_decision}"
                "crosswalk identity")

string(REPLACE
  "include/asc/random/quasi.h;src/random/quasi.cc"
  "include/asc/random/quasi.hsrc/random/quasi.cc"
  _changed_separator
  "${_valid_manifest}"
)
_expect_failure(changed-separator "${_changed_separator}"
                "evidence path does not exist")

string(REPLACE
  "crosswalk:"
  "unapproved_metadata: \"value\"\ncrosswalk:"
  _changed_schema
  "${_valid_manifest}"
)
_expect_failure(changed-schema "${_changed_schema}"
                "metadata fields differ from the frozen schema")

string(CONCAT _repeated_section "${_valid_manifest}" "\ncrosswalk:\n")
_expect_failure(repeated-section "${_repeated_section}"
                "repeats the crosswalk section")

set(_rejected_hashes
  230184f60bae2feaf244f10a8bac053c8ff33a183bcc365b4d8b876d2b7f4809
  3e562c25682c9fd1cb47dc700d3dc69366047a9228ce61472cca7143586f7046
  efb0ddedcb6a11ed3defd85764e352f16b09f13784c15225a1b2dd19dcbc2d6b
  9ce197d46b1dc35392abfbee31d96f2645ea773c72d9a2148ede41be8cfa5e04
  c1cf0ca3fc6496d971cef10a3214f06db68f9fa57ab85177c26550c9589122e1
  21495bdbb7578b0ad201cebf03198f2a840b1ca178ab9dd61ce8cd62a777af0e
  293480a20a9c811c994ed9b92ca2d1e88eb1645b82b0178c5923e0de97c15049
  bdf92696b098fc6ccfcf697ec0dbf928fd98da13fd45a7d90b73ddba9fabb88b
  750787ee8bfdc4a9c15f9327b9515a1efb6279fdce3ec37827345f17cc8c4d96
  c70853a78a93f1840690f7093734838db3b8932114e1e054e63ddced09fc22f3
  d77a3caac5830c6dbd155999a65497cbaf5eeaf0d01398f03fb7abe00a4fb7fd
  ebadbac5b74e47512642d02eac4e3d654d0aa260c3c79ec6d45e76df03f78a0a
  27cdce117132f9684bc932ce640d56b43ea45a799a1ea21f02719ffac3f78827
  9c61d4445824b9ee04793edbb6c4dbcf6aa9fe2c0d34b83feaeb3de8f2594bce
  f43157752ad613be02b0e5c2271fcd801f0f5695f6f4b85b2d17cf5a5f8ceda2
  16722f0f9978c1d4f5f0cfc67228f5417f217f4fa30bc7666a03e10d52bd3e14
  836668fdbb3b72fabc0f0d07febfa06d9b022bed59b51d8eb5e5deed32a4b3cd
  84ecc456307776a5c63fca9ddfca4ebf0088109c73a8f32fa1792661bedc462b
  0fe2b8f50993ded69bae47fbd1305011e1b699698a19ccd21cdf9c3cbfe8ae2f
  7f61b63f25b86deb3397f3ced5db0be27f7a612520ce04b1d3b233ea83e4c903
  6b2b56f09304362e9785f8e77bd12315ada5ee7081829a2eb395ea3f765358e8
)

file(GLOB_RECURSE _random_audit_files LIST_DIRECTORIES FALSE
  "${SOURCE_DIR}/include/asc/random/*.h"
  "${SOURCE_DIR}/src/random/*.cc"
  "${SOURCE_DIR}/src/random/*.cu"
  "${SOURCE_DIR}/src/random/CMakeLists.txt"
  "${SOURCE_DIR}/tests/random*/*.cc"
  "${SOURCE_DIR}/tests/random*/*.cu"
  "${SOURCE_DIR}/benchmarks/random*/*.cc"
  "${SOURCE_DIR}/data/random/*"
  "${SOURCE_DIR}/tools/random/*"
  "${SOURCE_DIR}/THIRD_PARTY_NOTICES"
)
set(_forbidden_fragments
  "MDECPP_"
  "class RandomSampler"
  "class LowDiscrepancyPermutation"
  "MDECPP_SOURCE_DIR"
  "sobol.bin"
  "sobol.txt"
  "time(nullptr)"
)
foreach(_audit_file IN LISTS _random_audit_files)
  file(SHA256 "${_audit_file}" _audit_sha256)
  if(_audit_sha256 IN_LIST _rejected_hashes)
    file(RELATIVE_PATH _relative "${SOURCE_DIR}" "${_audit_file}")
    message(FATAL_ERROR
      "Random file exactly copies a rejected artifact: ${_relative}"
    )
  endif()
  file(READ "${_audit_file}" _audit_contents)
  foreach(_forbidden IN LISTS _forbidden_fragments)
    string(FIND "${_audit_contents}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
      file(RELATIVE_PATH _relative "${SOURCE_DIR}" "${_audit_file}")
      message(FATAL_ERROR
        "Random file contains forbidden provenance text "
        "'${_forbidden}': ${_relative}"
      )
    endif()
  endforeach()
endforeach()

list(LENGTH _random_audit_files _random_audit_file_count)
message(STATUS
  "Random contract validates 33 decisions, generated-report drift, negative "
  "fixtures, and ${_random_audit_file_count} product/data/build files."
)
