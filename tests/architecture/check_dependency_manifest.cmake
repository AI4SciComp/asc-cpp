cmake_minimum_required(VERSION 3.25)

foreach(_required_variable IN ITEMS SOURCE_DIR DEPENDENCY_MANIFEST CAPABILITY_MANIFEST)
  if(NOT DEFINED "${_required_variable}" OR "${${_required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${_required_variable} is required.")
  endif()
endforeach()

foreach(_manifest IN ITEMS "${DEPENDENCY_MANIFEST}" "${CAPABILITY_MANIFEST}")
  if(NOT EXISTS "${_manifest}")
    message(FATAL_ERROR "Required manifest does not exist: ${_manifest}")
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
  output_dependency_declarations
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
      if(CMAKE_MATCH_1 STREQUAL requested_section)
        set(_active TRUE)
      else()
        set(_active FALSE)
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

  set("${output_nodes}" "${_nodes}" PARENT_SCOPE)
  set("${output_edges}" "${_edges}" PARENT_SCOPE)
  set(
    "${output_dependency_declarations}"
    "${_dependency_declarations}"
    PARENT_SCOPE
  )
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
  _module_dependency_declarations
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
  _facet_dependency_declarations
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
  _provider_dependency_declarations
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
_assert_exact_set(
  "module dependency declarations"
  "${_module_dependency_declarations}"
  "${_expected_modules}"
)
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
set(_expected_facet_owners
  "random_dense=random"
  "random_sparse=random"
  "cpp=package"
)
set(_expected_facet_module_flags
  "random_dense=false"
  "random_sparse=false"
  "cpp=false"
)
_assert_exact_set("facet names" "${_facet_nodes}" "${_expected_facets}")
_assert_exact_set(
  "facet dependency declarations"
  "${_facet_dependency_declarations}"
  "${_expected_facets}"
)
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
_assert_exact_set("facet owners" "${_facet_owners}" "${_expected_facet_owners}")
_assert_exact_set(
  "facet module flags"
  "${_facet_module_flags}"
  "${_expected_facet_module_flags}"
)

set(_expected_provider_facets
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
set(_expected_provider_owners
  "core_cuda=core"
  "dense_cuda=dense"
  "sparse_cuda=sparse"
  "random_cuda=random"
  "random_dense_cuda=random"
  "random_sparse_cuda=random"
)
_assert_exact_set(
  "provider facet names"
  "${_provider_nodes}"
  "${_expected_provider_facets}"
)
_assert_exact_set(
  "provider facet dependency declarations"
  "${_provider_dependency_declarations}"
  "${_expected_provider_facets}"
)
_assert_exact_set(
  "provider facet edges"
  "${_provider_edges}"
  "${_expected_provider_edges}"
)
_assert_exact_set(
  "provider facet build targets"
  "${_provider_build_targets}"
  "${_expected_provider_build_targets}"
)
_assert_exact_set(
  "provider facet installed targets"
  "${_provider_installed_targets}"
  "${_expected_provider_installed_targets}"
)
_assert_exact_set(
  "provider facet owners"
  "${_provider_owners}"
  "${_expected_provider_owners}"
)

file(STRINGS "${DEPENDENCY_MANIFEST}" _dependency_lines ENCODING UTF-8)
set(_reading_provider_free_components FALSE)
set(_provider_free_components)
foreach(_line IN LISTS _dependency_lines)
  if(_line MATCHES "^  provider_free_components:$")
    set(_reading_provider_free_components TRUE)
  elseif(_reading_provider_free_components
         AND _line MATCHES "^    - \"([a-z][a-z0-9_]*)\"$")
    list(APPEND _provider_free_components "${CMAKE_MATCH_1}")
  elseif(_reading_provider_free_components AND _line MATCHES "^  [a-z][a-z0-9_]*:")
    set(_reading_provider_free_components FALSE)
  endif()
endforeach()
set(_expected_provider_free_components
  core
  utilities
  expression
  dense
  sparse
  random
  random_dense
  random_sparse
  cpp
)
_assert_exact_set(
  "provider-free package components"
  "${_provider_free_components}"
  "${_expected_provider_free_components}"
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

file(STRINGS "${CAPABILITY_MANIFEST}" _capability_lines ENCODING UTF-8)
set(_capability_names)
set(_capability_owners)
set(_capability_facets)
set(_current_capability)
set(_current_owner)
set(_current_facet)
set(_current_dependencies)
set(_current_dependencies_declared FALSE)
set(_reading_capability_dependencies FALSE)

macro(_finalize_capability)
  if(_current_capability)
    if(NOT _current_owner IN_LIST _expected_modules)
      message(FATAL_ERROR
        "Capability '${_current_capability}' has invalid owner "
        "'${_current_owner}'."
      )
    endif()
    if(NOT _current_dependencies_declared)
      message(FATAL_ERROR
        "Capability '${_current_capability}' omits direct_dependencies."
      )
    endif()
    list(APPEND _capability_names "${_current_capability}")
    list(APPEND _capability_owners "${_current_owner}")

    if(_current_facet)
      if(NOT _current_facet IN_LIST _expected_facets
         AND NOT _current_facet IN_LIST _expected_provider_facets)
        message(FATAL_ERROR
          "Capability '${_current_capability}' has invalid facet "
          "'${_current_facet}'."
        )
      endif()
      set(_graph_key "${_current_facet}")
      list(APPEND _capability_facets "${_current_facet}")
      set(_owner_variable "_expected_owner_${_current_facet}")
      if(NOT DEFINED "${_owner_variable}"
         OR NOT _current_owner STREQUAL "${${_owner_variable}}")
        message(FATAL_ERROR
          "Capability '${_current_capability}' assigns facet "
          "'${_current_facet}' to '${_current_owner}', expected "
          "'${${_owner_variable}}'."
        )
      endif()
    else()
      set(_graph_key "${_current_owner}")
    endif()

    set(_dependency_variable "_expected_dependencies_${_graph_key}")
    _assert_exact_set(
      "dependencies of capability '${_current_capability}'"
      "${_current_dependencies}"
      "${${_dependency_variable}}"
    )
  endif()
endmacro()

foreach(_line IN LISTS _capability_lines)
  if(_line MATCHES "^  - capability: \"([^\"]+)\"$")
    _finalize_capability()
    set(_current_capability "${CMAKE_MATCH_1}")
    set(_current_owner)
    set(_current_facet)
    set(_current_dependencies)
    set(_current_dependencies_declared FALSE)
    set(_reading_capability_dependencies FALSE)
  elseif(_current_capability AND _line MATCHES "^    owner: \"([a-z][a-z0-9_]*)\"$")
    set(_current_owner "${CMAKE_MATCH_1}")
    set(_reading_capability_dependencies FALSE)
  elseif(_current_capability AND _line MATCHES "^    facet: \"([a-z][a-z0-9_]*)\"$")
    set(_current_facet "${CMAKE_MATCH_1}")
    set(_reading_capability_dependencies FALSE)
  elseif(_current_capability AND _line MATCHES "^    direct_dependencies: \\[\\]$")
    set(_current_dependencies_declared TRUE)
    set(_reading_capability_dependencies FALSE)
  elseif(_current_capability AND _line MATCHES "^    direct_dependencies:$")
    set(_current_dependencies_declared TRUE)
    set(_reading_capability_dependencies TRUE)
  elseif(_current_capability
         AND _reading_capability_dependencies
         AND _line MATCHES "^      - \"([a-z][a-z0-9_]*)\"$")
    list(APPEND _current_dependencies "${CMAKE_MATCH_1}")
  elseif(_current_capability AND _line MATCHES "^    [a-z][a-z0-9_]*:")
    set(_reading_capability_dependencies FALSE)
  endif()
endforeach()
_finalize_capability()

set(_expected_capability_names
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
  "${_expected_capability_names}"
)
set(_represented_capability_owners "${_capability_owners}")
list(REMOVE_DUPLICATES _represented_capability_owners)
_assert_exact_set(
  "capability owners represented"
  "${_represented_capability_owners}"
  "${_expected_modules}"
)
set(_represented_capability_facets "${_capability_facets}")
list(REMOVE_DUPLICATES _represented_capability_facets)
_assert_exact_set(
  "capability facets represented"
  "${_represented_capability_facets}"
  "random_dense;random_sparse;${_expected_provider_facets}"
)

set(_components_file "${SOURCE_DIR}/cmake/ASCCppComponents.cmake")
if(NOT EXISTS "${_components_file}")
  message(FATAL_ERROR "Missing component graph implementation: ${_components_file}")
endif()
include("${_components_file}")

_assert_exact_set(
  "implemented module component names"
  "${ASC_CPP_MODULE_COMPONENTS}"
  "${_expected_modules}"
)
_assert_exact_set(
  "implemented facet component names"
  "${ASC_CPP_FACET_COMPONENTS}"
  "random_dense;random_sparse"
)
_assert_exact_set(
  "implemented provider component names"
  "${ASC_CPP_PROVIDER_COMPONENTS}"
  "${_expected_provider_facets}"
)
_assert_exact_set(
  "implemented aggregate component names"
  "${ASC_CPP_AGGREGATE_COMPONENTS}"
  "cpp"
)
_assert_exact_set(
  "implemented known component names"
  "${ASC_CPP_KNOWN_COMPONENTS}"
  "${_expected_provider_free_components};${_expected_provider_facets}"
)
set(_expected_implemented_components
  ${_expected_provider_free_components}
  ${_expected_provider_facets}
)
foreach(_component IN LISTS _expected_implemented_components)
  set(_actual_variable "ASC_CPP_COMPONENT_${_component}_DEPENDENCIES")
  set(_expected_variable "_expected_dependencies_${_component}")
  if(DEFINED "${_actual_variable}")
    set(_actual_dependencies "${${_actual_variable}}")
  else()
    set(_actual_dependencies)
  endif()
  _assert_exact_set(
    "implemented dependencies of '${_component}'"
    "${_actual_dependencies}"
    "${${_expected_variable}}"
  )
endforeach()
unset(_expected_implemented_components)
