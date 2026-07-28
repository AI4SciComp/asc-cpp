if(BUILD_SHARED_LIBS)
  set(_asc_core_expected_library_type SHARED_LIBRARY)
else()
  set(_asc_core_expected_library_type STATIC_LIBRARY)
endif()

set(_asc_core_consumer_common_arguments
  "-DPRODUCER_BINARY_DIR:PATH=${PROJECT_BINARY_DIR}"
  "-DCONSUMER_SOURCE_DIR:PATH=${CMAKE_CURRENT_SOURCE_DIR}/core"
  "-DBUILD_PACKAGE_DIR:PATH=${ASC_CPP_BUILD_PACKAGE_DIR}"
  "-DGENERATOR:STRING=${CMAKE_GENERATOR}"
  "-DGENERATOR_PLATFORM:STRING=${CMAKE_GENERATOR_PLATFORM}"
  "-DGENERATOR_TOOLSET:STRING=${CMAKE_GENERATOR_TOOLSET}"
  "-DMAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}"
  "-DCONFIG:STRING=$<CONFIG>"
  "-DASCCPP_EXPECT_CUDA:BOOL=${ASC_CPP_ENABLE_CUDA}"
  "-DEXPECT_LIBRARY_TYPE:STRING=${_asc_core_expected_library_type}"
  "-DASC_CPP_TEST_WORKSPACE_ROOT:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}"
  "-DASC_CPP_TEST_WORKSPACE_GUARD:FILEPATH=${ASC_CPP_TEST_WORKSPACE_GUARD}"
)

add_test(
  NAME asc_cpp.consumer.core.build_tree
  COMMAND
    "${CMAKE_COMMAND}"
    ${_asc_core_consumer_common_arguments}
    "-DACTION:STRING=BUILD_TREE"
    "-DWORK_DIR:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}/consumer core build tree"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_core_consumer.cmake"
)
add_test(
  NAME asc_cpp.consumer.core.install_relocate
  COMMAND
    "${CMAKE_COMMAND}"
    ${_asc_core_consumer_common_arguments}
    "-DACTION:STRING=INSTALL_RELOCATE"
    "-DWORK_DIR:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}/consumer core install relocate"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_core_consumer.cmake"
)
add_test(
  NAME asc_cpp.consumer.foundations.subproject
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    "-DCONSUMER_SOURCE_DIR:PATH=${CMAKE_CURRENT_SOURCE_DIR}/subproject"
    "-DASCCMAKE_DIR:PATH=${ASCCMake_DIR}"
    "-DGENERATOR:STRING=${CMAKE_GENERATOR}"
    "-DGENERATOR_PLATFORM:STRING=${CMAKE_GENERATOR_PLATFORM}"
    "-DGENERATOR_TOOLSET:STRING=${CMAKE_GENERATOR_TOOLSET}"
    "-DMAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}"
    "-DCONFIG:STRING=$<CONFIG>"
    "-DENABLE_CUDA:BOOL=${ASC_CPP_ENABLE_CUDA}"
    "-DCUDA_ARCHITECTURES:STRING=${CMAKE_CUDA_ARCHITECTURES}"
    "-DASC_CPP_TEST_WORKSPACE_ROOT:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}"
    "-DASC_CPP_TEST_WORKSPACE_GUARD:FILEPATH=${ASC_CPP_TEST_WORKSPACE_GUARD}"
    "-DWORK_DIR:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}/consumer foundations subproject"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_subproject_consumer.cmake"
)
set_tests_properties(
  asc_cpp.consumer.core.build_tree
  asc_cpp.consumer.core.install_relocate
  asc_cpp.consumer.foundations.subproject
  PROPERTIES
    LABELS "consumer;core;package;milestone-7;milestone-8"
    RUN_SERIAL TRUE
    TIMEOUT 180
)

unset(_asc_core_consumer_common_arguments)
unset(_asc_core_expected_library_type)

if(BUILD_SHARED_LIBS)
  set(_asc_compiled_library_type SHARED_LIBRARY)
else()
  set(_asc_compiled_library_type STATIC_LIBRARY)
endif()

set(_asc_cpp_consumer_components
    utilities
    expression
    dense
    sparse
    random
    random_dense
    random_sparse
    cpp
)
if(ASC_CPP_ENABLE_CUDA)
  list(APPEND
    _asc_cpp_consumer_components
    core_cuda
    dense_cuda
    sparse_cuda
    random_cuda
    random_dense_cuda
    random_sparse_cuda
  )
endif()

foreach(_component IN LISTS _asc_cpp_consumer_components)
  if(_component MATCHES "^(expression|random_dense|random_sparse|cpp)$")
    set(_expected_library_type INTERFACE_LIBRARY)
  else()
    set(_expected_library_type "${_asc_compiled_library_type}")
  endif()
  if(_component STREQUAL "cpp")
    set(_executable_name asc_cpp_aggregate_consumer)
  else()
    set(_executable_name "asc_cpp_${_component}_consumer")
  endif()
  set(_consumer_common_arguments
    "-DBUILD_PACKAGE_DIR:PATH=${ASC_CPP_BUILD_PACKAGE_DIR}"
    "-DCOMPONENT:STRING=${_component}"
    "-DCONFIG:STRING=$<CONFIG>"
    "-DCONSUMER_SOURCE_DIR:PATH=${CMAKE_CURRENT_SOURCE_DIR}/${_component}"
    "-DEXECUTABLE_NAME:STRING=${_executable_name}"
    "-DASCCPP_EXPECT_CUDA:BOOL=${ASC_CPP_ENABLE_CUDA}"
    "-DEXPECT_LIBRARY_TYPE:STRING=${_expected_library_type}"
    "-DGENERATOR:STRING=${CMAKE_GENERATOR}"
    "-DGENERATOR_PLATFORM:STRING=${CMAKE_GENERATOR_PLATFORM}"
    "-DGENERATOR_TOOLSET:STRING=${CMAKE_GENERATOR_TOOLSET}"
    "-DMAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}"
    "-DPRODUCER_BINARY_DIR:PATH=${PROJECT_BINARY_DIR}"
    "-DASC_CPP_TEST_WORKSPACE_ROOT:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}"
    "-DASC_CPP_TEST_WORKSPACE_GUARD:FILEPATH=${ASC_CPP_TEST_WORKSPACE_GUARD}"
  )
  add_test(
    NAME "asc_cpp.consumer.${_component}.build_tree"
    COMMAND
      "${CMAKE_COMMAND}"
      ${_consumer_common_arguments}
      "-DACTION:STRING=BUILD_TREE"
      "-DWORK_DIR:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}/consumer ${_component} build tree"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/run_component_consumer.cmake"
  )
  add_test(
    NAME "asc_cpp.consumer.${_component}.install_relocate"
    COMMAND
      "${CMAKE_COMMAND}"
      ${_consumer_common_arguments}
      "-DACTION:STRING=INSTALL_RELOCATE"
      "-DWORK_DIR:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}/consumer ${_component} install relocate"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/run_component_consumer.cmake"
  )
  set_tests_properties(
    "asc_cpp.consumer.${_component}.build_tree"
    "asc_cpp.consumer.${_component}.install_relocate"
    PROPERTIES
      LABELS "consumer;${_component};package;milestone-7;milestone-8"
      RUN_SERIAL TRUE
      TIMEOUT 180
  )
endforeach()

unset(_asc_compiled_library_type)
unset(_asc_cpp_consumer_components)
unset(_component)
unset(_consumer_common_arguments)
unset(_executable_name)
unset(_expected_library_type)
