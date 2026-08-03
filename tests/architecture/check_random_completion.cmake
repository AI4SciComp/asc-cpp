cmake_minimum_required(VERSION 3.25)

foreach(_required_variable IN ITEMS
    SOURCE_DIR BINARY_DIR ENABLE_CUDA SUPPORTS_DISABLED_EXCEPTIONS
)
  if(NOT DEFINED "${_required_variable}"
     OR "${${_required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${_required_variable} is required.")
  endif()
endforeach()

function(_require_text relative_path expected)
  set(_path "${SOURCE_DIR}/${relative_path}")
  if(NOT EXISTS "${_path}")
    message(FATAL_ERROR "Random completion evidence is missing: ${_path}")
  endif()
  file(READ "${_path}" _contents)
  string(FIND "${_contents}" "${expected}" _position)
  if(_position EQUAL -1)
    message(FATAL_ERROR
      "Random completion evidence ${relative_path} omits: ${expected}"
    )
  endif()
endfunction()

_require_text(
  "docs/contracts/random-crosswalk.yaml"
  "contract_status: \"v0.9.0-release-contract\""
)
_require_text(
  "docs/contracts/random-crosswalk.yaml"
  "expected_incomplete: 0"
)
_require_text(
  "docs/contracts/random-crosswalk.yaml"
  "expected_permission_relicensing_required: 0"
)
_require_text(
  "docs/contracts/random-crosswalk.yaml"
  "expected_clean_room_required: 0"
)
_require_text(
  "docs/provenance/mdecpp-review.md"
  "all 33 inventory decisions"
)
_require_text(
  "docs/provenance/mdecpp-review.md"
  "11 remain rejected"
)
_require_text(
  "docs/architecture/decisions/0020-random-contract.md"
  "Seed, state, stream, and version contract"
)
_require_text(
  "docs/architecture/decisions/0020-random-contract.md"
  "Reproducible statistical verification"
)
_require_text(
  "docs/architecture/decisions/0020-random-contract.md"
  "thread-local mutable engine, seed"
)
_require_text(
  "release/release-notes-v0.9.0.md"
  "random generation, storage adapters"
)

set(_random_base_headers
  include/asc/random.h
  include/asc/random/distribution.h
  include/asc/random/engine.h
  include/asc/random/export.h
  include/asc/random/generator.h
  include/asc/random/quasi.h
  include/asc/random/seed.h
)
foreach(_header IN LISTS _random_base_headers)
  file(READ "${SOURCE_DIR}/${_header}" _contents)
  if(_contents MATCHES "asc/(dense|sparse)/")
    message(FATAL_ERROR
      "Storage-neutral Random header imports storage: ${_header}"
    )
  endif()
endforeach()
file(READ "${SOURCE_DIR}/include/asc/random/dense.h" _dense_contents)
if(_dense_contents MATCHES "asc/sparse/")
  message(FATAL_ERROR "Random Dense imports Sparse storage.")
endif()
file(READ "${SOURCE_DIR}/include/asc/random/sparse.h" _sparse_contents)
if(_sparse_contents MATCHES "asc/dense/")
  message(FATAL_ERROR "Random Sparse imports Dense storage.")
endif()

file(GLOB_RECURSE _random_product_files LIST_DIRECTORIES FALSE
  "${SOURCE_DIR}/include/asc/random.h"
  "${SOURCE_DIR}/include/asc/random/*.h"
  "${SOURCE_DIR}/src/random/*.cc"
  "${SOURCE_DIR}/src/random/*.cu"
)
foreach(_product_file IN LISTS _random_product_files)
  file(READ "${_product_file}" _contents)
  foreach(_forbidden IN ITEMS
      "thread_local"
      "class RandomSampler"
      "class LowDiscrepancyPermutation"
  )
    string(FIND "${_contents}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
      file(RELATIVE_PATH _relative "${SOURCE_DIR}" "${_product_file}")
      message(FATAL_ERROR
        "Random completion audit found forbidden hidden state/architecture "
        "'${_forbidden}' in ${_relative}."
      )
    endif()
  endforeach()
endforeach()

file(GLOB_RECURSE _ctest_files LIST_DIRECTORIES FALSE
  "${BINARY_DIR}/tests/*CTestTestfile.cmake"
)
if(NOT _ctest_files)
  message(FATAL_ERROR
    "No generated CTest inventory exists below ${BINARY_DIR}/tests."
  )
endif()
set(_ctest_inventory)
foreach(_ctest_file IN LISTS _ctest_files)
  file(READ "${_ctest_file}" _ctest_contents)
  string(APPEND _ctest_inventory "${_ctest_contents}\n")
endforeach()

set(_required_tests
  asc_cpp.architecture.dependency_manifest
  asc_cpp.architecture.approved_product_targets
  asc_cpp.architecture.public_file_policy
  asc_cpp.architecture.random_contract
  asc_cpp.architecture.random_completion
  asc_cpp.compile.m2_header.asc_random_h
  asc_cpp.compile.m2_header.asc_random_distribution_h
  asc_cpp.compile.m2_header.asc_random_engine_h
  asc_cpp.compile.m2_header.asc_random_export_h
  asc_cpp.compile.m2_header.asc_random_generator_h
  asc_cpp.compile.m2_header.asc_random_quasi_h
  asc_cpp.compile.m2_header.asc_random_seed_h
  asc_cpp.compile.m5_header.asc_random_dense_h
  asc_cpp.compile.m5_header.asc_random_sparse_h
  asc_cpp.compile.m5_random_dense_header
  asc_cpp.compile.m5_random_sparse_header
  asc_cpp.random.distribution_test
  asc_cpp.random.engine_test
  asc_cpp.random.generator_test
  asc_cpp.random.seed_test
  asc_cpp.random.stateful_engine_test
  asc_cpp.random.statistical_test
  asc_cpp.random.quasi_test
  asc_cpp.random.sobol_artifacts
  asc_cpp.random.generator_benchmark
  asc_cpp.random_dense.advanced_sampler_test
  asc_cpp.random_dense.dense_generation_test
  asc_cpp.random_dense.qmc_fill_test
  asc_cpp.random_dense.thread_partition_test
  asc_cpp.random_dense.allocation_test
  asc_cpp.random_sparse.random_adapter_test
  asc_cpp.random_sparse.sparse_generation_test
  asc_cpp.random_sparse.thread_reproducibility_test
  asc_cpp.random_sparse.allocation_test
  asc_cpp.random_storage.benchmark
  asc_cpp.consumer.random.build_tree
  asc_cpp.consumer.random.install_relocate
  asc_cpp.consumer.random_dense.build_tree
  asc_cpp.consumer.random_dense.install_relocate
  asc_cpp.consumer.random_sparse.build_tree
  asc_cpp.consumer.random_sparse.install_relocate
  asc_cpp.package.build_tree_components
  asc_cpp.package.install_and_relocate_components
  asc_cpp.hardening.header_manifest.build_tree
  asc_cpp.hardening.header_manifest.installed
  asc_cpp.hardening.header_manifest.relocated
  asc_cpp.hardening.documentation_consistency
  asc_cpp.downstream.asc_xde.build_tree
  asc_cpp.downstream.asc_xde.relocated
)
if(SUPPORTS_DISABLED_EXCEPTIONS)
  list(APPEND _required_tests
    asc_cpp.compile.m2_header_no_exceptions.asc_random_h
    asc_cpp.compile.m2_header_no_exceptions.asc_random_distribution_h
    asc_cpp.compile.m2_header_no_exceptions.asc_random_engine_h
    asc_cpp.compile.m2_header_no_exceptions.asc_random_export_h
    asc_cpp.compile.m2_header_no_exceptions.asc_random_generator_h
    asc_cpp.compile.m2_header_no_exceptions.asc_random_quasi_h
    asc_cpp.compile.m2_header_no_exceptions.asc_random_seed_h
    asc_cpp.compile.m5_header_no_exceptions.asc_random_dense_h
    asc_cpp.compile.m5_header_no_exceptions.asc_random_sparse_h
  )
endif()
if(ENABLE_CUDA)
  list(APPEND _required_tests
    asc_cpp.compile.m7_header.asc_random_providers_cuda_h
    asc_cpp.compile.m7_header.asc_random_providers_cuda_export_h
    asc_cpp.compile.m7_header.asc_random_providers_dense_cuda_h
    asc_cpp.compile.m7_header.asc_random_providers_dense_cuda_export_h
    asc_cpp.compile.m7_header.asc_random_providers_sparse_cuda_h
    asc_cpp.compile.m7_header.asc_random_providers_sparse_cuda_export_h
    asc_cpp.compile.m7_random_cuda_header
    asc_cpp.compile.m7_random_dense_cuda_header
    asc_cpp.compile.m7_random_sparse_cuda_header
    asc_cpp.random_cuda.runtime
    asc_cpp.random_cuda.benchmark
    asc_cpp.random_dense_cuda.runtime
    asc_cpp.random_sparse_cuda.runtime
    asc_cpp.random_sparse_cuda.priority_ordinal_collision
    asc_cpp.consumer.random_cuda.build_tree
    asc_cpp.consumer.random_cuda.install_relocate
    asc_cpp.consumer.random_dense_cuda.build_tree
    asc_cpp.consumer.random_dense_cuda.install_relocate
    asc_cpp.consumer.random_sparse_cuda.build_tree
    asc_cpp.consumer.random_sparse_cuda.install_relocate
  )
  if(SUPPORTS_DISABLED_EXCEPTIONS)
    list(APPEND _required_tests
      asc_cpp.compile.m7_header_no_exceptions.asc_random_providers_cuda_h
      asc_cpp.compile.m7_header_no_exceptions.asc_random_providers_cuda_export_h
      asc_cpp.compile.m7_header_no_exceptions.asc_random_providers_dense_cuda_h
      asc_cpp.compile.m7_header_no_exceptions.asc_random_providers_dense_cuda_export_h
      asc_cpp.compile.m7_header_no_exceptions.asc_random_providers_sparse_cuda_h
      asc_cpp.compile.m7_header_no_exceptions.asc_random_providers_sparse_cuda_export_h
    )
  endif()
endif()

foreach(_required_test IN LISTS _required_tests)
  # CMake's generated CTest syntax varies by version: test names may be
  # bracket-quoted, ordinarily quoted, or unquoted. Match the registration
  # itself instead of depending on one serialization.
  set(_position -1)
  foreach(_test_registration IN ITEMS
      "add_test([=[${_required_test}]=]"
      "add_test(\"${_required_test}\""
      "add_test(${_required_test} "
  )
    string(FIND "${_ctest_inventory}" "${_test_registration}" _position)
    if(NOT _position EQUAL -1)
      break()
    endif()
  endforeach()
  if(_position EQUAL -1)
    message(FATAL_ERROR
      "Random completion evidence test is not registered: ${_required_test}"
    )
  endif()
endforeach()

list(LENGTH _required_tests _required_test_count)
list(LENGTH _random_product_files _random_product_file_count)
message(STATUS
  "Random completion audit validated ${_required_test_count} evidence "
  "registrations and ${_random_product_file_count} product files."
)
