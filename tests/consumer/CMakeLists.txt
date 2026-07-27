if(BUILD_SHARED_LIBS)
  set(_asc_core_expected_library_type SHARED_LIBRARY)
else()
  set(_asc_core_expected_library_type STATIC_LIBRARY)
endif()

set(_asc_core_consumer_common_arguments
  "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
  "-DPRODUCER_BINARY_DIR:PATH=${PROJECT_BINARY_DIR}"
  "-DCONSUMER_SOURCE_DIR:PATH=${CMAKE_CURRENT_SOURCE_DIR}/core"
  "-DBUILD_PACKAGE_DIR:PATH=${ASC_CPP_BUILD_PACKAGE_DIR}"
  "-DGENERATOR:STRING=${CMAKE_GENERATOR}"
  "-DGENERATOR_PLATFORM:STRING=${CMAKE_GENERATOR_PLATFORM}"
  "-DGENERATOR_TOOLSET:STRING=${CMAKE_GENERATOR_TOOLSET}"
  "-DMAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}"
  "-DCONFIG:STRING=$<CONFIG>"
  "-DEXPECT_LIBRARY_TYPE:STRING=${_asc_core_expected_library_type}"
  "-DENABLE_CUDA:BOOL=${ASC_CPP_ENABLE_CUDA}"
)

add_test(
  NAME asc_cpp.consumer.core.build_tree
  COMMAND
    "${CMAKE_COMMAND}"
    ${_asc_core_consumer_common_arguments}
    "-DACTION:STRING=BUILD_TREE"
    "-DWORK_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}/core build tree work"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_core_consumer.cmake"
)
add_test(
  NAME asc_cpp.consumer.core.install_relocate
  COMMAND
    "${CMAKE_COMMAND}"
    ${_asc_core_consumer_common_arguments}
    "-DACTION:STRING=INSTALL_RELOCATE"
    "-DWORK_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}/core install relocate work"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_core_consumer.cmake"
)
add_test(
  NAME asc_cpp.consumer.cpp.subproject
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
    "-DWORK_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}/cpp subproject work"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_subproject_consumer.cmake"
)
set_tests_properties(
  asc_cpp.consumer.core.build_tree
  asc_cpp.consumer.core.install_relocate
  PROPERTIES
    LABELS "consumer;core;package;milestone-1"
    RUN_SERIAL TRUE
    TIMEOUT 180
)
set_tests_properties(
  asc_cpp.consumer.cpp.subproject
  PROPERTIES
    LABELS "consumer;cpp;package;milestone-6"
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

foreach(_component IN ITEMS
    utilities
    expression
    random
    dense
    sparse
    random_dense
    random_sparse
    cpp
)
  if(_component STREQUAL "expression"
     OR _component STREQUAL "random_dense"
     OR _component STREQUAL "random_sparse"
     OR _component STREQUAL "cpp")
    set(_expected_library_type INTERFACE_LIBRARY)
  else()
    set(_expected_library_type "${_asc_compiled_library_type}")
  endif()
  set(_consumer_common_arguments
    "-DBUILD_PACKAGE_DIR:PATH=${ASC_CPP_BUILD_PACKAGE_DIR}"
    "-DCOMPONENT:STRING=${_component}"
    "-DCONFIG:STRING=$<CONFIG>"
    "-DCONSUMER_SOURCE_DIR:PATH=${CMAKE_CURRENT_SOURCE_DIR}/${_component}"
    "-DEXECUTABLE_NAME:STRING=asc_cpp_${_component}_consumer"
    "-DEXPECT_LIBRARY_TYPE:STRING=${_expected_library_type}"
    "-DGENERATOR:STRING=${CMAKE_GENERATOR}"
    "-DGENERATOR_PLATFORM:STRING=${CMAKE_GENERATOR_PLATFORM}"
    "-DGENERATOR_TOOLSET:STRING=${CMAKE_GENERATOR_TOOLSET}"
    "-DMAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}"
    "-DPRODUCER_BINARY_DIR:PATH=${PROJECT_BINARY_DIR}"
    "-DENABLE_CUDA:BOOL=${ASC_CPP_ENABLE_CUDA}"
  )
  add_test(
    NAME "asc_cpp.consumer.${_component}.build_tree"
    COMMAND
      "${CMAKE_COMMAND}"
      ${_consumer_common_arguments}
      "-DACTION:STRING=BUILD_TREE"
      "-DWORK_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}/${_component} build tree work"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/run_component_consumer.cmake"
  )
  add_test(
    NAME "asc_cpp.consumer.${_component}.install_relocate"
    COMMAND
      "${CMAKE_COMMAND}"
      ${_consumer_common_arguments}
      "-DACTION:STRING=INSTALL_RELOCATE"
      "-DWORK_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}/${_component} install relocate work"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/run_component_consumer.cmake"
  )
  set_tests_properties(
    "asc_cpp.consumer.${_component}.build_tree"
    "asc_cpp.consumer.${_component}.install_relocate"
    PROPERTIES
      LABELS "consumer;${_component};package;milestone-6"
      RUN_SERIAL TRUE
      TIMEOUT 180
  )
endforeach()

if(ASC_CPP_ENABLE_CUDA)
  foreach(_component IN ITEMS
      core_cuda
      dense_cuda
      sparse_cuda
      random_cuda
      random_dense_cuda
      random_sparse_cuda
  )
    if(_component STREQUAL "core_cuda"
       OR _component STREQUAL "dense_cuda")
      set(_cuda_milestone_label milestone-6)
    else()
      set(_cuda_milestone_label milestone-7)
    endif()
    set(_consumer_common_arguments
      "-DBUILD_PACKAGE_DIR:PATH=${ASC_CPP_BUILD_PACKAGE_DIR}"
      "-DCOMPONENT:STRING=${_component}"
      "-DCONFIG:STRING=$<CONFIG>"
      "-DCONSUMER_SOURCE_DIR:PATH=${CMAKE_CURRENT_SOURCE_DIR}/${_component}"
      "-DEXECUTABLE_NAME:STRING=asc_cpp_${_component}_consumer"
      "-DEXPECT_LIBRARY_TYPE:STRING=${_asc_compiled_library_type}"
      "-DGENERATOR:STRING=${CMAKE_GENERATOR}"
      "-DGENERATOR_PLATFORM:STRING=${CMAKE_GENERATOR_PLATFORM}"
      "-DGENERATOR_TOOLSET:STRING=${CMAKE_GENERATOR_TOOLSET}"
      "-DMAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}"
      "-DPRODUCER_BINARY_DIR:PATH=${PROJECT_BINARY_DIR}"
      "-DENABLE_CUDA:BOOL=${ASC_CPP_ENABLE_CUDA}"
    )
    add_test(
      NAME "asc_cpp.consumer.${_component}.build_tree"
      COMMAND
        "${CMAKE_COMMAND}"
        ${_consumer_common_arguments}
        "-DACTION:STRING=BUILD_TREE"
        "-DWORK_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}/${_component} build tree work"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_component_consumer.cmake"
    )
    add_test(
      NAME "asc_cpp.consumer.${_component}.install_relocate"
      COMMAND
        "${CMAKE_COMMAND}"
        ${_consumer_common_arguments}
        "-DACTION:STRING=INSTALL_RELOCATE"
        "-DWORK_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}/${_component} install relocate work"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_component_consumer.cmake"
    )
    set_tests_properties(
      "asc_cpp.consumer.${_component}.build_tree"
      "asc_cpp.consumer.${_component}.install_relocate"
      PROPERTIES
        LABELS "consumer;gpu;${_component};package;${_cuda_milestone_label}"
        RUN_SERIAL TRUE
        TIMEOUT 240
    )
  endforeach()
endif()

unset(_asc_compiled_library_type)
unset(_component)
unset(_consumer_common_arguments)
unset(_cuda_milestone_label)
unset(_expected_library_type)
