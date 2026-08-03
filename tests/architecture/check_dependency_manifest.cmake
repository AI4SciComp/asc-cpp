cmake_minimum_required(VERSION 3.25)

foreach(_required_variable IN ITEMS
    SOURCE_DIR
    DEPENDENCY_MANIFEST
    CAPABILITY_MANIFEST
)
  if(NOT DEFINED "${_required_variable}"
     OR "${${_required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${_required_variable} is required.")
  endif()
endforeach()

foreach(_manifest IN ITEMS "${DEPENDENCY_MANIFEST}" "${CAPABILITY_MANIFEST}")
  if(NOT EXISTS "${_manifest}")
    message(FATAL_ERROR "Required manifest does not exist: ${_manifest}")
  endif()
  file(READ "${_manifest}" _manifest_contents)
  if(NOT _manifest_contents MATCHES "(^|\n)schema_version: 1(\n|$)")
    message(FATAL_ERROR
      "Unsupported or missing schema_version in ${_manifest}."
    )
  endif()
endforeach()

function(_assert_exact_set label actual expected)
  set(_actual "${actual}")
  set(_expected "${expected}")

  list(LENGTH _actual _actual_length)
  list(REMOVE_DUPLICATES _actual)
  list(LENGTH _actual _unique_length)
  if(NOT _actual_length EQUAL _unique_length)
    message(FATAL_ERROR "${label} contains duplicate entries: ${actual}")
  endif()

  list(SORT _actual)
  list(SORT _expected)
  if(NOT _actual STREQUAL _expected)
    message(FATAL_ERROR
      "${label} differs from the frozen contract.\n"
      "  expected: ${_expected}\n"
      "  actual:   ${_actual}"
    )
  endif()
endfunction()

function(
  _parse_graph_section
  manifest
  requested_section
  output_nodes
  output_edges
  output_build_targets
  output_installed_targets
  output_owners
  output_module_flags
)
  file(STRINGS "${manifest}" _lines ENCODING UTF-8)
  set(_active FALSE)
  set(_current_node)
  set(_reading_dependencies FALSE)
  set(_nodes)
  set(_edges)
  set(_dependency_declarations)
  set(_build_targets)
  set(_installed_targets)
  set(_owners)
  set(_module_flags)

  foreach(_line IN LISTS _lines)
    if(_line MATCHES "^([a-z_]+):$")
      set(_active FALSE)
      if(CMAKE_MATCH_1 STREQUAL requested_section)
        set(_active TRUE)
      endif()
      set(_current_node)
      set(_reading_dependencies FALSE)
    elseif(_active AND _line MATCHES "^  ([a-z][a-z0-9_]*):$")
      set(_current_node "${CMAKE_MATCH_1}")
      list(APPEND _nodes "${_current_node}")
      set(_reading_dependencies FALSE)
    elseif(_active AND _current_node)
      if(_line MATCHES "^    direct_dependencies: \\[\\]$")
        list(APPEND _dependency_declarations "${_current_node}")
        set(_reading_dependencies FALSE)
      elseif(_line MATCHES "^    direct_dependencies:$")
        list(APPEND _dependency_declarations "${_current_node}")
        set(_reading_dependencies TRUE)
      elseif(_reading_dependencies
             AND _line MATCHES "^      - \"([a-z][a-z0-9_]*)\"$")
        list(APPEND _edges "${_current_node}>${CMAKE_MATCH_1}")
      elseif(_line MATCHES "^    build_target: \"([^\"]+)\"$")
        set(_reading_dependencies FALSE)
        list(APPEND _build_targets "${_current_node}=${CMAKE_MATCH_1}")
      elseif(_line MATCHES "^    installed_target: \"([^\"]+)\"$")
        set(_reading_dependencies FALSE)
        list(APPEND _installed_targets "${_current_node}=${CMAKE_MATCH_1}")
      elseif(_line MATCHES "^    owner: \"([a-z][a-z0-9_]*)\"$")
        set(_reading_dependencies FALSE)
        list(APPEND _owners "${_current_node}=${CMAKE_MATCH_1}")
      elseif(_line MATCHES "^    is_module: (true|false)$")
        set(_reading_dependencies FALSE)
        list(APPEND _module_flags "${_current_node}=${CMAKE_MATCH_1}")
      elseif(_line MATCHES "^    [a-z][a-z0-9_]*:")
        set(_reading_dependencies FALSE)
      endif()
    endif()
  endforeach()

  _assert_exact_set(
    "${requested_section} dependency declarations"
    "${_dependency_declarations}"
    "${_nodes}"
  )
  set("${output_nodes}" "${_nodes}" PARENT_SCOPE)
  set("${output_edges}" "${_edges}" PARENT_SCOPE)
  set("${output_build_targets}" "${_build_targets}" PARENT_SCOPE)
  set("${output_installed_targets}" "${_installed_targets}" PARENT_SCOPE)
  set("${output_owners}" "${_owners}" PARENT_SCOPE)
  set("${output_module_flags}" "${_module_flags}" PARENT_SCOPE)
endfunction()

_parse_graph_section(
  "${DEPENDENCY_MANIFEST}"
  modules
  _module_nodes
  _module_edges
  _module_build_targets
  _module_installed_targets
  _unused_module_owners
  _unused_module_flags
)
_parse_graph_section(
  "${DEPENDENCY_MANIFEST}"
  facets
  _facet_nodes
  _facet_edges
  _facet_build_targets
  _facet_installed_targets
  _facet_owners
  _facet_module_flags
)
_parse_graph_section(
  "${DEPENDENCY_MANIFEST}"
  provider_facets
  _provider_nodes
  _provider_edges
  _provider_build_targets
  _provider_installed_targets
  _provider_owners
  _unused_provider_flags
)

set(_expected_modules core utilities expression dense sparse random)
set(_expected_module_edges
  "utilities>core"
  "expression>core"
  "dense>core"
  "dense>expression"
  "sparse>core"
  "sparse>expression"
  "random>core"
)
set(_expected_module_build_targets
  "core=asc_core"
  "utilities=asc_utilities"
  "expression=asc_expression"
  "dense=asc_dense"
  "sparse=asc_sparse"
  "random=asc_random"
)
set(_expected_module_installed_targets
  "core=ASC::core"
  "utilities=ASC::utilities"
  "expression=ASC::expression"
  "dense=ASC::dense"
  "sparse=ASC::sparse"
  "random=ASC::random"
)
_assert_exact_set("module names" "${_module_nodes}" "${_expected_modules}")
_assert_exact_set("module edges" "${_module_edges}" "${_expected_module_edges}")
_assert_exact_set(
  "module build targets"
  "${_module_build_targets}"
  "${_expected_module_build_targets}"
)
_assert_exact_set(
  "module installed targets"
  "${_module_installed_targets}"
  "${_expected_module_installed_targets}"
)

set(_expected_facets random_dense random_sparse cpp)
set(_expected_facet_edges
  "random_dense>random"
  "random_dense>dense"
  "random_sparse>random"
  "random_sparse>sparse"
  "cpp>core"
  "cpp>utilities"
  "cpp>expression"
  "cpp>dense"
  "cpp>sparse"
  "cpp>random"
  "cpp>random_dense"
  "cpp>random_sparse"
)
set(_expected_facet_build_targets
  "random_dense=asc_random_dense"
  "random_sparse=asc_random_sparse"
  "cpp=asc_cpp"
)
set(_expected_facet_installed_targets
  "random_dense=ASC::random_dense"
  "random_sparse=ASC::random_sparse"
  "cpp=ASC::cpp"
)
_assert_exact_set("facet names" "${_facet_nodes}" "${_expected_facets}")
_assert_exact_set("facet edges" "${_facet_edges}" "${_expected_facet_edges}")
_assert_exact_set(
  "facet build targets"
  "${_facet_build_targets}"
  "${_expected_facet_build_targets}"
)
_assert_exact_set(
  "facet installed targets"
  "${_facet_installed_targets}"
  "${_expected_facet_installed_targets}"
)
_assert_exact_set(
  "facet owners"
  "${_facet_owners}"
  "random_dense=random;random_sparse=random;cpp=package"
)
_assert_exact_set(
  "facet module flags"
  "${_facet_module_flags}"
  "random_dense=false;random_sparse=false;cpp=false"
)

set(_expected_providers
  core_cuda
  dense_cuda
  sparse_cuda
  random_cuda
  random_dense_cuda
  random_sparse_cuda
)
set(_expected_provider_edges
  "core_cuda>core"
  "dense_cuda>dense"
  "dense_cuda>core_cuda"
  "sparse_cuda>sparse"
  "sparse_cuda>core_cuda"
  "random_cuda>random"
  "random_cuda>core_cuda"
  "random_dense_cuda>random_dense"
  "random_dense_cuda>random_cuda"
  "random_dense_cuda>core_cuda"
  "random_sparse_cuda>random_sparse"
  "random_sparse_cuda>random_cuda"
  "random_sparse_cuda>core_cuda"
)
set(_expected_provider_build_targets
  "core_cuda=asc_core_cuda"
  "dense_cuda=asc_dense_cuda"
  "sparse_cuda=asc_sparse_cuda"
  "random_cuda=asc_random_cuda"
  "random_dense_cuda=asc_random_dense_cuda"
  "random_sparse_cuda=asc_random_sparse_cuda"
)
set(_expected_provider_installed_targets
  "core_cuda=ASC::core_cuda"
  "dense_cuda=ASC::dense_cuda"
  "sparse_cuda=ASC::sparse_cuda"
  "random_cuda=ASC::random_cuda"
  "random_dense_cuda=ASC::random_dense_cuda"
  "random_sparse_cuda=ASC::random_sparse_cuda"
)
_assert_exact_set(
  "provider names"
  "${_provider_nodes}"
  "${_expected_providers}"
)
_assert_exact_set(
  "provider edges"
  "${_provider_edges}"
  "${_expected_provider_edges}"
)
_assert_exact_set(
  "provider build targets"
  "${_provider_build_targets}"
  "${_expected_provider_build_targets}"
)
_assert_exact_set(
  "provider installed targets"
  "${_provider_installed_targets}"
  "${_expected_provider_installed_targets}"
)
set(_expected_provider_owners
  "core_cuda=core"
  "dense_cuda=dense"
  "sparse_cuda=sparse"
  "random_cuda=random"
  "random_dense_cuda=random"
  "random_sparse_cuda=random"
)
_assert_exact_set(
  "provider owners" "${_provider_owners}" "${_expected_provider_owners}"
)

set(_expected_dependencies_core)
set(_expected_dependencies_utilities core)
set(_expected_dependencies_expression core)
set(_expected_dependencies_dense core expression)
set(_expected_dependencies_sparse core expression)
set(_expected_dependencies_random core)
set(_expected_dependencies_random_dense random dense)
set(_expected_dependencies_random_sparse random sparse)
set(_expected_dependencies_cpp
  core utilities expression dense sparse random random_dense random_sparse
)
set(_expected_dependencies_core_cuda core)
set(_expected_dependencies_dense_cuda dense core_cuda)
set(_expected_dependencies_sparse_cuda sparse core_cuda)
set(_expected_dependencies_random_cuda random core_cuda)
set(_expected_dependencies_random_dense_cuda random_dense random_cuda core_cuda)
set(_expected_dependencies_random_sparse_cuda
  random_sparse random_cuda core_cuda
)
set(_expected_owner_random_dense random)
set(_expected_owner_random_sparse random)
set(_expected_owner_core_cuda core)
set(_expected_owner_dense_cuda dense)
set(_expected_owner_sparse_cuda sparse)
set(_expected_owner_random_cuda random)
set(_expected_owner_random_dense_cuda random)
set(_expected_owner_random_sparse_cuda random)

file(STRINGS "${DEPENDENCY_MANIFEST}" _dependency_lines ENCODING UTF-8)
set(_reading_provider_free_components FALSE)
set(_provider_free_components)
foreach(_line IN LISTS _dependency_lines)
  if(_line MATCHES "^  provider_free_components:$")
    set(_reading_provider_free_components TRUE)
  elseif(_reading_provider_free_components
         AND _line MATCHES "^    - \"([a-z][a-z0-9_]*)\"$")
    list(APPEND _provider_free_components "${CMAKE_MATCH_1}")
  elseif(_reading_provider_free_components
         AND _line MATCHES "^  [a-z][a-z0-9_]*:")
    set(_reading_provider_free_components FALSE)
  endif()
endforeach()
set(_expected_provider_free_components
  core utilities expression dense sparse random random_dense random_sparse cpp
)
_assert_exact_set(
  "provider-free package components"
  "${_provider_free_components}"
  "${_expected_provider_free_components}"
)

foreach(_package_list IN ITEMS implemented_components unavailable_components)
  set(_reading_package_list FALSE)
  set("_manifest_${_package_list}")
  foreach(_line IN LISTS _dependency_lines)
    if(_line MATCHES "^  ${_package_list}:$")
      set(_reading_package_list TRUE)
    elseif(_reading_package_list
           AND _line MATCHES "^    - \"([a-z][a-z0-9_]*)\"$")
      list(APPEND "_manifest_${_package_list}" "${CMAKE_MATCH_1}")
    elseif(_reading_package_list AND _line MATCHES "^  [a-z][a-z0-9_]*:")
      set(_reading_package_list FALSE)
    endif()
  endforeach()
endforeach()
_assert_exact_set(
  "implemented package components"
  "${_manifest_implemented_components}"
  "core;utilities;expression;dense;sparse;random;random_dense;random_sparse;cpp;core_cuda;dense_cuda;sparse_cuda;random_cuda;random_dense_cuda;random_sparse_cuda"
)
_assert_exact_set(
  "unavailable package components"
  "${_manifest_unavailable_components}"
  ""
)

file(STRINGS "${CAPABILITY_MANIFEST}" _capability_lines ENCODING UTF-8)
set(_capability_names)
set(_capability_owners)
set(_capability_facets)
set(_current_capability)
set(_current_owner)
set(_current_facet)
set(_current_dependencies)
set(_dependencies_declared FALSE)
set(_reading_dependencies FALSE)

macro(_finalize_capability)
  if(_current_capability)
    if(NOT _current_owner IN_LIST _expected_modules)
      message(FATAL_ERROR
        "Capability '${_current_capability}' has invalid owner "
        "'${_current_owner}'."
      )
    endif()
    if(NOT _dependencies_declared)
      message(FATAL_ERROR
        "Capability '${_current_capability}' omits direct_dependencies."
      )
    endif()
    if(_current_facet)
      set(_graph_key "${_current_facet}")
      if(NOT _current_facet IN_LIST _expected_facets
         AND NOT _current_facet IN_LIST _expected_providers)
        message(FATAL_ERROR
          "Capability '${_current_capability}' has invalid facet "
          "'${_current_facet}'."
        )
      endif()
      set(_owner_variable "_expected_owner_${_current_facet}")
      if(NOT DEFINED "${_owner_variable}"
         OR NOT _current_owner STREQUAL "${${_owner_variable}}")
        message(FATAL_ERROR
          "Capability '${_current_capability}' has the wrong facet owner."
        )
      endif()
      list(APPEND _capability_facets "${_current_facet}")
    else()
      set(_graph_key "${_current_owner}")
    endif()
    set(_dependency_variable "_expected_dependencies_${_graph_key}")
    _assert_exact_set(
      "dependencies of capability '${_current_capability}'"
      "${_current_dependencies}"
      "${${_dependency_variable}}"
    )
    list(APPEND _capability_names "${_current_capability}")
    list(APPEND _capability_owners "${_current_owner}")
  endif()
endmacro()

foreach(_line IN LISTS _capability_lines)
  if(_line MATCHES "^  - capability: \"([^\"]+)\"$")
    _finalize_capability()
    set(_current_capability "${CMAKE_MATCH_1}")
    set(_current_owner)
    set(_current_facet)
    set(_current_dependencies)
    set(_dependencies_declared FALSE)
    set(_reading_dependencies FALSE)
  elseif(_current_capability
         AND _line MATCHES "^    owner: \"([a-z][a-z0-9_]*)\"$")
    set(_current_owner "${CMAKE_MATCH_1}")
    set(_reading_dependencies FALSE)
  elseif(_current_capability
         AND _line MATCHES "^    facet: \"([a-z][a-z0-9_]*)\"$")
    set(_current_facet "${CMAKE_MATCH_1}")
    set(_reading_dependencies FALSE)
  elseif(_current_capability
         AND _line MATCHES "^    direct_dependencies: \\[\\]$")
    set(_dependencies_declared TRUE)
    set(_reading_dependencies FALSE)
  elseif(_current_capability AND _line MATCHES "^    direct_dependencies:$")
    set(_dependencies_declared TRUE)
    set(_reading_dependencies TRUE)
  elseif(_current_capability
         AND _reading_dependencies
         AND _line MATCHES "^      - \"([a-z][a-z0-9_]*)\"$")
    list(APPEND _current_dependencies "${CMAKE_MATCH_1}")
  elseif(_current_capability AND _line MATCHES "^    [a-z][a-z0-9_]*:")
    set(_reading_dependencies FALSE)
  endif()
endforeach()
_finalize_capability()

set(_expected_capabilities
  "checked logical metadata and mixed extents"
  "status, result, and release-active contracts"
  "storage-independent configuration model"
  "portable byte/text I/O primitives"
  "host memory resource and move-only raw buffer"
  "serial execution context and completion event"
  "transactional command-line configuration"
  "monotonic timer"
  "storage-neutral pointwise expression protocol"
  "Philox4x32-10 raw-bit and Uniform01 contract"
  "storage-neutral engines and scalar distributions"
  "storage-neutral QMC sequences"
  "dense owner and strided mutable/const views"
  "dense pointwise evaluation and reductions"
  "serial dense BLAS subset"
  "general-rank coordinate sparse storage"
  "rank-two CSR and CSC"
  "sparse expression evaluation and serial CSR SpMV"
  "deterministic dense random fill"
  "deterministic sparse exact-count generation"
  "CUDA resources, copies, streams, and events"
  "CUDA dense evaluation and selected cuBLAS"
  "CUDA sparse evaluation and selected cuSPARSE operations"
  "CUDA random raw-bit provider"
  "CUDA dense random generation"
  "CUDA sparse random generation"
)
_assert_exact_set(
  "capability names"
  "${_capability_names}"
  "${_expected_capabilities}"
)
set(_represented_owners "${_capability_owners}")
list(REMOVE_DUPLICATES _represented_owners)
_assert_exact_set(
  "capability owners"
  "${_represented_owners}"
  "${_expected_modules}"
)
set(_represented_facets "${_capability_facets}")
list(REMOVE_DUPLICATES _represented_facets)
_assert_exact_set(
  "capability facets"
  "${_represented_facets}"
  "random_dense;random_sparse;${_expected_providers}"
)

set(_current_capability)
set(_current_development_stage)
set(_current_status)
macro(_assert_capability_status)
  if(_current_capability)
    if(_current_development_stage LESS_EQUAL 5
       OR _current_capability STREQUAL
          "CUDA resources, copies, streams, and events")
      set(_expected_status runtime-tested)
    elseif(_current_capability STREQUAL
           "CUDA dense evaluation and selected cuBLAS")
      set(_expected_status parity-tested)
    elseif(_current_development_stage EQUAL 7)
      set(_expected_status parity-tested)
    elseif(_current_capability STREQUAL
           "storage-neutral engines and scalar distributions"
           OR _current_capability STREQUAL
              "storage-neutral QMC sequences")
      set(_expected_status runtime-tested)
    else()
      set(_expected_status proposed)
    endif()
    if(NOT _current_status STREQUAL _expected_status)
      message(FATAL_ERROR
        "Capability '${_current_capability}' at development stage "
        "${_current_development_stage} must be '${_expected_status}', found "
        "'${_current_status}'."
      )
    endif()
  endif()
endmacro()
foreach(_line IN LISTS _capability_lines)
  if(_line MATCHES "^  - capability: \"([^\"]+)\"$")
    _assert_capability_status()
    set(_current_capability "${CMAKE_MATCH_1}")
    set(_current_development_stage)
    set(_current_status)
  elseif(_current_capability
         AND _line MATCHES "^    development_stage: ([0-9]+)$")
    set(_current_development_stage "${CMAKE_MATCH_1}")
  elseif(_current_capability
         AND _line MATCHES "^    status: \"([a-z-]+)\"$")
    set(_current_status "${CMAKE_MATCH_1}")
  endif()
endforeach()
_assert_capability_status()

set(_components_file "${SOURCE_DIR}/cmake/ASCCppComponents.cmake")
if(NOT EXISTS "${_components_file}")
  message(FATAL_ERROR "Missing component graph: ${_components_file}")
endif()
include("${_components_file}")
_assert_exact_set(
  "live module components"
  "${ASC_CPP_MODULE_COMPONENTS}"
  "${_expected_modules}"
)
_assert_exact_set(
  "live facet components"
  "${ASC_CPP_FACET_COMPONENTS}"
  "random_dense;random_sparse"
)
_assert_exact_set(
  "live provider components"
  "${ASC_CPP_PROVIDER_COMPONENTS}"
  "${_expected_providers}"
)
_assert_exact_set(
  "live aggregate components"
  "${ASC_CPP_AGGREGATE_COMPONENTS}"
  "cpp"
)
_assert_exact_set(
  "live known components"
  "${ASC_CPP_KNOWN_COMPONENTS}"
  "${_expected_provider_free_components};${_expected_providers}"
)
foreach(_component IN LISTS ASC_CPP_KNOWN_COMPONENTS)
  set(_actual_variable "ASC_CPP_COMPONENT_${_component}_DEPENDENCIES")
  set(_expected_variable "_expected_dependencies_${_component}")
  _assert_exact_set(
    "live dependencies of ${_component}"
    "${${_actual_variable}}"
    "${${_expected_variable}}"
  )
endforeach()
