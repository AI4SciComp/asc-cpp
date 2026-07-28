cmake_path(
  IS_PREFIX PROJECT_SOURCE_DIR
  "${PROJECT_BINARY_DIR}"
  NORMALIZE
  _m8_binary_inside_source
)
if(_m8_binary_inside_source)
  set(_m8_temporary_root)
  foreach(_m8_temporary_variable IN ITEMS TMPDIR TEMP TMP)
    if(DEFINED ENV{${_m8_temporary_variable}}
       AND IS_DIRECTORY "$ENV{${_m8_temporary_variable}}")
      set(_m8_temporary_root "$ENV{${_m8_temporary_variable}}")
      break()
    endif()
  endforeach()
  if(NOT _m8_temporary_root AND UNIX AND IS_DIRECTORY /tmp)
    set(_m8_temporary_root /tmp)
  endif()
  if(NOT _m8_temporary_root)
    message(FATAL_ERROR
      "An out-of-source temporary directory is required for M8 reports."
    )
  endif()
  string(SHA256 _m8_build_hash "${PROJECT_BINARY_DIR}")
  string(SUBSTRING "${_m8_build_hash}" 0 16 _m8_build_hash)
  set(_m8_hardening_report_dir
    "${_m8_temporary_root}/asc-cpp-m8-hardening-${_m8_build_hash}"
  )
else()
  set(_m8_hardening_report_dir
    "${CMAKE_CURRENT_BINARY_DIR}/reports"
  )
endif()
file(MAKE_DIRECTORY "${_m8_hardening_report_dir}")

include("${PROJECT_SOURCE_DIR}/tools/hardening/TargetInventory.cmake")
asc_cpp_hardening_check_target_inventory(
  BASELINE "${PROJECT_SOURCE_DIR}/abi/targets-and-components.txt"
  HEADER_OWNERS "${PROJECT_SOURCE_DIR}/abi/header-owners.txt"
  OUTPUT "${_m8_hardening_report_dir}/target-inventory.txt"
)

set(_m8_fixture_root
  "${ASC_CPP_TEST_WORKSPACE_ROOT}/hardening package fixtures"
)
set(_m8_copied_build_package
  "${_m8_fixture_root}/copied build package with spaces/ASCCpp"
)
set(_m8_install_prefix
  "${_m8_fixture_root}/installed prefix with spaces"
)
set(_m8_relocated_prefix
  "${_m8_fixture_root}/relocated prefix with spaces"
)
if(DEFINED CMAKE_INSTALL_LIBDIR AND NOT CMAKE_INSTALL_LIBDIR STREQUAL "")
  set(_m8_install_libdir "${CMAKE_INSTALL_LIBDIR}")
else()
  set(_m8_install_libdir lib)
endif()
if(DEFINED CMAKE_INSTALL_INCLUDEDIR
   AND NOT CMAKE_INSTALL_INCLUDEDIR STREQUAL "")
  set(_m8_install_includedir "${CMAKE_INSTALL_INCLUDEDIR}")
else()
  set(_m8_install_includedir include)
endif()
set(_m8_install_package
  "${_m8_install_prefix}/${_m8_install_libdir}/cmake/ASCCpp"
)
set(_m8_relocated_package
  "${_m8_relocated_prefix}/${_m8_install_libdir}/cmake/ASCCpp"
)
set(_m8_install_include
  "${_m8_install_prefix}/${_m8_install_includedir}"
)
set(_m8_relocated_include
  "${_m8_relocated_prefix}/${_m8_install_includedir}"
)

set(
  ASC_CPP_M8_COPIED_BUILD_PACKAGE
  "${_m8_copied_build_package}"
  CACHE INTERNAL "M8 copied build-tree package used by downstream tests"
)
set(
  ASC_CPP_M8_INSTALL_PACKAGE
  "${_m8_install_package}"
  CACHE INTERNAL "M8 installed package used by downstream tests"
)
set(
  ASC_CPP_M8_RELOCATED_PACKAGE
  "${_m8_relocated_package}"
  CACHE INTERNAL "M8 relocated package used by downstream tests"
)

add_test(
  NAME asc_cpp.hardening.clean_copied_build_package
  COMMAND
    "${CMAKE_COMMAND}"
    "-DWORK_DIR:PATH=${_m8_fixture_root}/copied build package with spaces"
    "-DROOT:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}"
    "-DGUARD:FILEPATH=${ASC_CPP_TEST_WORKSPACE_GUARD}"
    -P "${PROJECT_SOURCE_DIR}/tests/cmake/RemoveTestWorkspace.cmake"
)
add_test(
  NAME asc_cpp.hardening.copy_build_package
  COMMAND
    "${CMAKE_COMMAND}" -E copy_directory
    "${ASC_CPP_BUILD_PACKAGE_DIR}"
    "${_m8_copied_build_package}"
)
set_tests_properties(
  asc_cpp.hardening.clean_copied_build_package
  PROPERTIES
    FIXTURES_SETUP M8CopiedBuildPackageClean
    LABELS "hardening;package;milestone-8"
    RUN_SERIAL TRUE
)
set_tests_properties(
  asc_cpp.hardening.copy_build_package
  PROPERTIES
    FIXTURES_REQUIRED M8CopiedBuildPackageClean
    FIXTURES_SETUP M8CopiedBuildPackage
    LABELS "hardening;package;milestone-8"
    RUN_SERIAL TRUE
)

add_test(
  NAME asc_cpp.hardening.clean_install_prefix
  COMMAND
    "${CMAKE_COMMAND}"
    "-DWORK_DIR:PATH=${_m8_install_prefix}"
    "-DROOT:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}"
    "-DGUARD:FILEPATH=${ASC_CPP_TEST_WORKSPACE_GUARD}"
    -P "${PROJECT_SOURCE_DIR}/tests/cmake/RemoveTestWorkspace.cmake"
)
set(_m8_install_command
  "${CMAKE_COMMAND}" --install "${PROJECT_BINARY_DIR}"
  --prefix "${_m8_install_prefix}"
)
if(CMAKE_CONFIGURATION_TYPES)
  list(APPEND _m8_install_command --config "$<CONFIG>")
endif()
add_test(
  NAME asc_cpp.hardening.install_package
  COMMAND ${_m8_install_command}
)
set_tests_properties(
  asc_cpp.hardening.clean_install_prefix
  PROPERTIES
    FIXTURES_SETUP M8InstallPrefixClean
    LABELS "hardening;package;milestone-8"
    RUN_SERIAL TRUE
)
set_tests_properties(
  asc_cpp.hardening.install_package
  PROPERTIES
    FIXTURES_REQUIRED M8InstallPrefixClean
    FIXTURES_SETUP M8InstalledPackage
    LABELS "hardening;package;milestone-8"
    RUN_SERIAL TRUE
    TIMEOUT 180
)

add_test(
  NAME asc_cpp.hardening.clean_relocated_prefix
  COMMAND
    "${CMAKE_COMMAND}"
    "-DWORK_DIR:PATH=${_m8_relocated_prefix}"
    "-DROOT:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}"
    "-DGUARD:FILEPATH=${ASC_CPP_TEST_WORKSPACE_GUARD}"
    -P "${PROJECT_SOURCE_DIR}/tests/cmake/RemoveTestWorkspace.cmake"
)
add_test(
  NAME asc_cpp.hardening.relocate_package
  COMMAND
    "${CMAKE_COMMAND}" -E copy_directory
    "${_m8_install_prefix}"
    "${_m8_relocated_prefix}"
)
set_tests_properties(
  asc_cpp.hardening.clean_relocated_prefix
  PROPERTIES
    FIXTURES_REQUIRED M8InstalledPackage
    FIXTURES_SETUP M8RelocatedPrefixClean
    LABELS "hardening;package;relocation;milestone-8"
    RUN_SERIAL TRUE
)
set_tests_properties(
  asc_cpp.hardening.relocate_package
  PROPERTIES
    FIXTURES_REQUIRED "M8InstalledPackage;M8RelocatedPrefixClean"
    FIXTURES_SETUP M8RelocatedPackage
    LABELS "hardening;package;relocation;milestone-8"
    RUN_SERIAL TRUE
)

add_test(
  NAME asc_cpp.hardening.public_surface.source
  COMMAND
    "${CMAKE_COMMAND}"
    "-DASC_CPP_HARDENING_SOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    "-DASC_CPP_HARDENING_OUTPUT:FILEPATH=${_m8_hardening_report_dir}/public-surface-source.txt"
    -P "${PROJECT_SOURCE_DIR}/tools/hardening/CheckPublicSurface.cmake"
)
set_tests_properties(
  asc_cpp.hardening.public_surface.source
  PROPERTIES LABELS "hardening;api;package;milestone-8"
)

function(_asc_cpp_add_m8_package_metadata_test
    name package_directory fixture)
  add_test(
    NAME "asc_cpp.hardening.package_metadata.${name}"
    COMMAND
      "${CMAKE_COMMAND}"
      "-DASCCPP_PACKAGE_DIR:PATH=${package_directory}"
      "-DWORK_DIR:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}/hardening ${name} metadata"
      "-DASC_CPP_TEST_WORKSPACE_ROOT:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}"
      "-DASC_CPP_TEST_WORKSPACE_GUARD:FILEPATH=${ASC_CPP_TEST_WORKSPACE_GUARD}"
      "-DEXPECT_CUDA:BOOL=${ASC_CPP_ENABLE_CUDA}"
      "-DEXPECT_SHARED:BOOL=${BUILD_SHARED_LIBS}"
      "-DTEST_GENERATOR:STRING=${CMAKE_GENERATOR}"
      "-DCXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
      -P
      "${CMAKE_CURRENT_SOURCE_DIR}/package_metadata_version_test.cmake"
  )
  set_tests_properties(
    "asc_cpp.hardening.package_metadata.${name}"
    PROPERTIES
      LABELS "hardening;package;api;consumer;milestone-8"
      RUN_SERIAL TRUE
      TIMEOUT 300
  )
  if(fixture)
    set_tests_properties(
      "asc_cpp.hardening.package_metadata.${name}"
      PROPERTIES FIXTURES_REQUIRED "${fixture}"
    )
  endif()
endfunction()

_asc_cpp_add_m8_package_metadata_test(
  build_tree "${ASC_CPP_BUILD_PACKAGE_DIR}" ""
)
_asc_cpp_add_m8_package_metadata_test(
  copied_build_tree
  "${_m8_copied_build_package}"
  M8CopiedBuildPackage
)
_asc_cpp_add_m8_package_metadata_test(
  installed "${_m8_install_package}" M8InstalledPackage
)
_asc_cpp_add_m8_package_metadata_test(
  relocated "${_m8_relocated_package}" M8RelocatedPackage
)

function(_asc_cpp_add_m8_header_manifest_test
    name package_directory install_include fixture)
  set(_command
    "${CMAKE_COMMAND}"
    "-DASCCPP_PACKAGE_DIR:PATH=${package_directory}"
    "-DSOURCE_INCLUDE_DIR:PATH=${PROJECT_SOURCE_DIR}/include"
    "-DWORK_DIR:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}/hardening ${name} headers"
    "-DASC_CPP_TEST_WORKSPACE_ROOT:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}"
    "-DASC_CPP_TEST_WORKSPACE_GUARD:FILEPATH=${ASC_CPP_TEST_WORKSPACE_GUARD}"
    "-DEXPECT_CUDA:BOOL=${ASC_CPP_ENABLE_CUDA}"
    "-DTEST_GENERATOR:STRING=${CMAKE_GENERATOR}"
  )
  if(install_include)
    list(APPEND _command "-DINSTALL_INCLUDE_DIR:PATH=${install_include}")
  endif()
  list(APPEND _command
    -P "${CMAKE_CURRENT_SOURCE_DIR}/header_manifest_test.cmake"
  )
  add_test(
    NAME "asc_cpp.hardening.header_manifest.${name}"
    COMMAND ${_command}
  )
  set_tests_properties(
    "asc_cpp.hardening.header_manifest.${name}"
    PROPERTIES
      LABELS "hardening;package;api;milestone-8"
      RUN_SERIAL TRUE
      TIMEOUT 180
  )
  if(fixture)
    set_tests_properties(
      "asc_cpp.hardening.header_manifest.${name}"
      PROPERTIES FIXTURES_REQUIRED "${fixture}"
    )
  endif()
endfunction()

_asc_cpp_add_m8_header_manifest_test(
  build_tree "${ASC_CPP_BUILD_PACKAGE_DIR}" "" ""
)
_asc_cpp_add_m8_header_manifest_test(
  copied_build_tree
  "${_m8_copied_build_package}"
  ""
  M8CopiedBuildPackage
)
_asc_cpp_add_m8_header_manifest_test(
  installed
  "${_m8_install_package}"
  "${_m8_install_include}"
  M8InstalledPackage
)
_asc_cpp_add_m8_header_manifest_test(
  relocated
  "${_m8_relocated_package}"
  "${_m8_relocated_include}"
  M8RelocatedPackage
)

add_test(
  NAME asc_cpp.hardening.public_surface.installed
  COMMAND
    "${CMAKE_COMMAND}"
    "-DASC_CPP_HARDENING_SOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    "-DASC_CPP_HARDENING_INSTALLED_INCLUDE_DIR:PATH=${_m8_relocated_include}"
    "-DASC_CPP_HARDENING_OUTPUT:FILEPATH=${_m8_hardening_report_dir}/public-surface-installed.txt"
    -P "${PROJECT_SOURCE_DIR}/tools/hardening/CheckPublicSurface.cmake"
)
set_tests_properties(
  asc_cpp.hardening.public_surface.installed
  PROPERTIES
    FIXTURES_REQUIRED M8RelocatedPackage
    LABELS "hardening;api;package;relocation;milestone-8"
    RUN_SERIAL TRUE
)

add_executable(
  asc_m8_public_api_surface_test
  public_api_surface_test.cc
)
target_link_libraries(asc_m8_public_api_surface_test PRIVATE ASC::dense)
if(ASC_CPP_ENABLE_CUDA)
  target_link_libraries(
    asc_m8_public_api_surface_test
    PRIVATE ASC::random_cuda
  )
  target_compile_definitions(
    asc_m8_public_api_surface_test
    PRIVATE ASC_M8_VERIFY_CUDA
  )
endif()
asc_target_enable_cxx20(TARGET asc_m8_public_api_surface_test)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(
    TARGET asc_m8_public_api_surface_test
    AS_ERRORS
  )
else()
  asc_target_enable_warnings(TARGET asc_m8_public_api_surface_test)
endif()
if(ASC_CPP_SANITIZER_ARGUMENTS)
  asc_target_enable_sanitizers(
    TARGET asc_m8_public_api_surface_test
    ${ASC_CPP_SANITIZER_ARGUMENTS}
  )
endif()
asc_register_test(
  NAME asc_cpp.hardening.public_api_surface
  TARGET asc_m8_public_api_surface_test
  LABELS "hardening;api;runtime;milestone-8"
)

add_test(
  NAME asc_cpp.hardening.documentation_consistency
  COMMAND
    "${CMAKE_COMMAND}"
    "-DASC_CPP_SOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    "-DASC_CPP_EXPECTED_PREDECESSOR_COMMIT:STRING=33b261ea33616a6395c4ad3b20646093103344f7"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/documentation_consistency_test.cmake"
)
set_tests_properties(
  asc_cpp.hardening.documentation_consistency
  PROPERTIES LABELS "hardening;documentation;security;milestone-8"
)

add_test(
  NAME asc_cpp.hardening.elf_baseline_selection
  COMMAND
    "${CMAKE_COMMAND}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/select_elf_baseline_test.cmake"
)

if(ASC_CPP_ENABLE_CUDA)
  find_program(_m8_baseline_gnu_compiler NAMES g++-11 g++)
  find_program(
    _m8_unlike_cuda_host_compiler
    NAMES clang++-19
  )
  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.31"
     AND _m8_baseline_gnu_compiler
     AND _m8_unlike_cuda_host_compiler)
    add_test(
      NAME asc_cpp.hardening.cuda_host_elf_selector_integration
      COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
        "-DWORK_DIR:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}/cuda host selector integration"
        "-DROOT:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}"
        "-DGUARD:FILEPATH=${ASC_CPP_TEST_WORKSPACE_GUARD}"
        "-DGENERATOR:STRING=${CMAKE_GENERATOR}"
        "-DMAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}"
        "-DCXX_COMPILER:FILEPATH=${_m8_baseline_gnu_compiler}"
        "-DCUDA_HOST_COMPILER:FILEPATH=${_m8_unlike_cuda_host_compiler}"
        "-DCUDA_ARCHITECTURES:STRING=${CMAKE_CUDA_ARCHITECTURES}"
        "-DASCCMAKE_DIR:PATH=${ASCCMake_DIR}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/cuda_host_elf_selector_integration_test.cmake"
    )
    set_tests_properties(
      asc_cpp.hardening.cuda_host_elf_selector_integration
      PROPERTIES
        LABELS "hardening;api;abi;cuda;portability;milestone-8"
        RUN_SERIAL TRUE
        TIMEOUT 180
    )
  endif()
endif()
set_tests_properties(
  asc_cpp.hardening.elf_baseline_selection
  PROPERTIES LABELS "hardening;api;abi;portability;milestone-8"
)

if(BUILD_SHARED_LIBS)
  find_program(_m8_nm NAMES nm llvm-nm)
  find_program(_m8_cxxfilt NAMES c++filt llvm-cxxfilt)
  find_program(_m8_readelf NAMES readelf llvm-readelf)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND _m8_nm AND _m8_cxxfilt)
    set(_m8_shared_symbol_command
      "${CMAKE_COMMAND}"
      "-DNM_EXECUTABLE:FILEPATH=${_m8_nm}"
      "-DCXXFILT_EXECUTABLE:FILEPATH=${_m8_cxxfilt}"
      "-DOUTPUT_FILE:FILEPATH=${CMAKE_CURRENT_BINARY_DIR}/representative-shared-symbols-$<CONFIG>.txt"
      "-DCORE_LIBRARY:FILEPATH=$<TARGET_FILE:asc_core>"
      "-DUTILITIES_LIBRARY:FILEPATH=$<TARGET_FILE:asc_utilities>"
      "-DDENSE_LIBRARY:FILEPATH=$<TARGET_FILE:asc_dense>"
      "-DSPARSE_LIBRARY:FILEPATH=$<TARGET_FILE:asc_sparse>"
      "-DRANDOM_LIBRARY:FILEPATH=$<TARGET_FILE:asc_random>"
    )
    if(ASC_CPP_ENABLE_CUDA)
      list(APPEND _m8_shared_symbol_command
        "-DCORE_CUDA_LIBRARY:FILEPATH=$<TARGET_FILE:asc_core_cuda>"
        "-DDENSE_CUDA_LIBRARY:FILEPATH=$<TARGET_FILE:asc_dense_cuda>"
        "-DSPARSE_CUDA_LIBRARY:FILEPATH=$<TARGET_FILE:asc_sparse_cuda>"
        "-DRANDOM_CUDA_LIBRARY:FILEPATH=$<TARGET_FILE:asc_random_cuda>"
        "-DRANDOM_DENSE_CUDA_LIBRARY:FILEPATH=$<TARGET_FILE:asc_random_dense_cuda>"
        "-DRANDOM_SPARSE_CUDA_LIBRARY:FILEPATH=$<TARGET_FILE:asc_random_sparse_cuda>"
      )
    endif()
    list(APPEND _m8_shared_symbol_command
      -P "${CMAKE_CURRENT_SOURCE_DIR}/shared_symbol_test.cmake"
    )
    add_test(
      NAME asc_cpp.hardening.representative_shared_symbols
      COMMAND ${_m8_shared_symbol_command}
    )
    set_tests_properties(
      asc_cpp.hardening.representative_shared_symbols
      PROPERTIES
        LABELS "hardening;api;abi;milestone-8"
        RUN_SERIAL TRUE
        TIMEOUT 120
    )
  endif()

  set(_m8_shared_libraries
    "$<TARGET_FILE:asc_core>"
    "$<TARGET_FILE:asc_utilities>"
    "$<TARGET_FILE:asc_dense>"
    "$<TARGET_FILE:asc_sparse>"
    "$<TARGET_FILE:asc_random>"
  )
  if(ASC_CPP_ENABLE_CUDA)
    list(APPEND _m8_shared_libraries
      "$<TARGET_FILE:asc_core_cuda>"
      "$<TARGET_FILE:asc_dense_cuda>"
      "$<TARGET_FILE:asc_sparse_cuda>"
      "$<TARGET_FILE:asc_random_cuda>"
      "$<TARGET_FILE:asc_random_dense_cuda>"
      "$<TARGET_FILE:asc_random_sparse_cuda>"
    )
  endif()
  set(_m8_distribution)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    cmake_host_system_information(
      RESULT _m8_distribution
      QUERY DISTRIB_PRETTY_NAME
    )
  endif()
  set(_m8_standard_library unknown)
  if("stdc++" IN_LIST CMAKE_CXX_IMPLICIT_LINK_LIBRARIES)
    set(_m8_standard_library libstdc++)
  elseif("c++" IN_LIST CMAKE_CXX_IMPLICIT_LINK_LIBRARIES)
    set(_m8_standard_library libc++)
  endif()
  set(_m8_standard_library_identity unknown)
  if(_m8_standard_library STREQUAL "libstdc++")
    execute_process(
      COMMAND "${CMAKE_CXX_COMPILER}" -print-file-name=libstdc++.so
      OUTPUT_VARIABLE _m8_standard_library_path
      ERROR_QUIET
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(IS_ABSOLUTE "${_m8_standard_library_path}"
       AND EXISTS "${_m8_standard_library_path}")
      file(REAL_PATH
        "${_m8_standard_library_path}"
        _m8_standard_library_real_path
      )
      cmake_path(GET
        _m8_standard_library_real_path
        FILENAME
        _m8_standard_library_filename
      )
      file(SHA256
        "${_m8_standard_library_real_path}"
        _m8_standard_library_sha256
      )
      set(_m8_standard_library_identity
        "libstdc++:${_m8_standard_library_filename}:sha256=${_m8_standard_library_sha256}"
      )
    endif()
  endif()
  set(_m8_c_library_identity unknown)
  find_program(_m8_ldd NAMES ldd)
  if(_m8_ldd)
    execute_process(
      COMMAND "${_m8_ldd}" --version
      OUTPUT_VARIABLE _m8_ldd_version
      ERROR_QUIET
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REGEX MATCH "^[^\r\n]*" _m8_c_library_identity
      "${_m8_ldd_version}"
    )
  endif()
  set(_m8_readelf_identity)
  set(_m8_nm_identity)
  set(_m8_cxxfilt_identity)
  foreach(_m8_tool IN ITEMS readelf nm cxxfilt)
    if(_m8_${_m8_tool})
      execute_process(
        COMMAND "${_m8_${_m8_tool}}" --version
        OUTPUT_VARIABLE _m8_tool_version
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )
      string(REGEX MATCH "^[^\r\n]*" _m8_tool_identity
        "${_m8_tool_version}"
      )
      set(_m8_${_m8_tool}_identity "${_m8_tool_identity}")
    endif()
  endforeach()
  include("${CMAKE_CURRENT_SOURCE_DIR}/SelectElfBaseline.cmake")
  set(_m8_trusted_cuda_host_compiler_id)
  set(_m8_trusted_cuda_host_compiler_version)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.31")
    set(_m8_trusted_cuda_host_compiler_id
      "${CMAKE_CUDA_HOST_COMPILER_ID}"
    )
    set(_m8_trusted_cuda_host_compiler_version
      "${CMAKE_CUDA_HOST_COMPILER_VERSION}"
    )
  endif()
  asc_cpp_select_elf_baseline(
    OUTPUT_VARIABLE _m8_elf_baseline
    BASELINE_DIRECTORY "${PROJECT_SOURCE_DIR}/abi"
    SYSTEM_NAME "${CMAKE_SYSTEM_NAME}"
    PROCESSOR "${CMAKE_SYSTEM_PROCESSOR}"
    DISTRIBUTION "${_m8_distribution}"
    CXX_COMPILER_ID "${CMAKE_CXX_COMPILER_ID}"
    CXX_COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}"
    STANDARD_LIBRARY "${_m8_standard_library}"
    STANDARD_LIBRARY_IDENTITY "${_m8_standard_library_identity}"
    C_LIBRARY_IDENTITY "${_m8_c_library_identity}"
    BUILD_TYPE "${CMAKE_BUILD_TYPE}"
    SHARED "${BUILD_SHARED_LIBS}"
    CUDA_ENABLED "${ASC_CPP_ENABLE_CUDA}"
    CUDA_COMPILER_ID "${CMAKE_CUDA_COMPILER_ID}"
    CUDA_COMPILER_VERSION "${CMAKE_CUDA_COMPILER_VERSION}"
    CUDA_HOST_COMPILER_ID "${_m8_trusted_cuda_host_compiler_id}"
    CUDA_HOST_COMPILER_VERSION "${_m8_trusted_cuda_host_compiler_version}"
    CUDA_ARCHITECTURES "${CMAKE_CUDA_ARCHITECTURES}"
    READELF_IDENTITY "${_m8_readelf_identity}"
    NM_IDENTITY "${_m8_nm_identity}"
    CXXFILT_IDENTITY "${_m8_cxxfilt_identity}"
    MULTI_CONFIG "${CMAKE_CONFIGURATION_TYPES}"
  )
  if(ASC_CPP_ENABLE_CUDA AND CMAKE_VERSION VERSION_LESS "3.31")
    add_test(
      NAME asc_cpp.hardening.cuda_host_elf_selector_unavailable
      COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
        "-DCMAKE_VERSION_UNDER_TEST:STRING=${CMAKE_VERSION}"
        "-DCUDA_HOST_COMPILER_ID:STRING=${_m8_trusted_cuda_host_compiler_id}"
        "-DCUDA_HOST_COMPILER_VERSION:STRING=${_m8_trusted_cuda_host_compiler_version}"
        "-DRAW_CUDA_HOST_COMPILER_ID:STRING=${CMAKE_CUDA_HOST_COMPILER_ID}"
        "-DRAW_CUDA_HOST_COMPILER_VERSION:STRING=${CMAKE_CUDA_HOST_COMPILER_VERSION}"
        "-DSELECTED_BASELINE:FILEPATH=${_m8_elf_baseline}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/cuda_host_elf_selector_unavailable_test.cmake"
    )
    set_tests_properties(
      asc_cpp.hardening.cuda_host_elf_selector_unavailable
      PROPERTIES
        LABELS "hardening;api;abi;cuda;portability;milestone-8"
    )
    add_test(
      NAME asc_cpp.hardening.cuda_host_elf_selector_untrusted_cache_integration
      COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
        "-DWORK_DIR:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}/cuda host selector untrusted cache integration"
        "-DROOT:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}"
        "-DGUARD:FILEPATH=${ASC_CPP_TEST_WORKSPACE_GUARD}"
        "-DGENERATOR:STRING=${CMAKE_GENERATOR}"
        "-DMAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}"
        "-DCXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
        "-DCUDA_HOST_COMPILER:FILEPATH=${CMAKE_CUDA_HOST_COMPILER}"
        "-DCUDA_ARCHITECTURES:STRING=${CMAKE_CUDA_ARCHITECTURES}"
        "-DASCCMAKE_DIR:PATH=${ASCCMake_DIR}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/cuda_host_elf_selector_untrusted_cache_integration_test.cmake"
    )
    set_tests_properties(
      asc_cpp.hardening.cuda_host_elf_selector_untrusted_cache_integration
      PROPERTIES
        LABELS "hardening;api;abi;cuda;portability;milestone-8"
        RUN_SERIAL TRUE
        TIMEOUT 180
    )
  endif()
  add_test(
    NAME asc_cpp.hardening.elf_abi
    COMMAND
      "${CMAKE_COMMAND}"
      "-DASC_CPP_HARDENING_SOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
      "-DASC_CPP_HARDENING_LIBRARIES:STRING=${_m8_shared_libraries}"
      "-DASC_CPP_HARDENING_OUTPUT:FILEPATH=${_m8_hardening_report_dir}/elf-abi-$<CONFIG>.txt"
      "-DASC_CPP_HARDENING_BASELINE:FILEPATH=${_m8_elf_baseline}"
      -P "${PROJECT_SOURCE_DIR}/tools/hardening/InspectElfAbi.cmake"
  )
  set_tests_properties(
    asc_cpp.hardening.elf_abi
    PROPERTIES
      LABELS "hardening;api;abi;milestone-8"
      RUN_SERIAL TRUE
      SKIP_REGULAR_EXPRESSION "ASCCpp ELF ABI inspection skipped"
      TIMEOUT 120
  )
endif()

function(_asc_cpp_add_m8_compile_observation name compiler style flags)
  add_test(
    NAME "asc_cpp.hardening.compile_object.${name}"
    COMMAND
      "${CMAKE_COMMAND}"
      "-DCXX_COMPILER:FILEPATH=${compiler}"
      "-DCOMPILER_STYLE:STRING=${style}"
      "-DINCLUDE_DIR:PATH=${_m8_relocated_include}"
      "-DWORK_DIR:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}/hardening compile object ${name}"
      "-DOBSERVATION_FILE:FILEPATH=${CMAKE_CURRENT_BINARY_DIR}/compile-object-${name}.txt"
      "-DOBSERVATION_BUILD_MODE:STRING=${_m8_observation_build_mode}"
      "-DCOMPILE_FLAGS:STRING=${flags}"
      -P
      "${PROJECT_SOURCE_DIR}/benchmarks/hardening/observe_compile_object.cmake"
  )
  set_tests_properties(
    "asc_cpp.hardening.compile_object.${name}"
    PROPERTIES
      LABELS "hardening;performance;compile;milestone-8"
      FIXTURES_REQUIRED M8RelocatedPackage
      RUN_SERIAL TRUE
      TIMEOUT 120
  )
endfunction()

if(CMAKE_CONFIGURATION_TYPES)
  set(_m8_observation_build_mode
    "standalone compile observation ($<CONFIG> package)"
  )
else()
  set(_m8_observation_build_mode "${CMAKE_BUILD_TYPE}")
endif()
set(_m8_configured_compiler_style gnu)
set(_m8_configured_compile_flags
  "-Wall;-Wextra;-Wpedantic;-Werror"
)
if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  set(_m8_configured_compiler_style msvc)
  set(_m8_configured_compile_flags "/W4;/WX;/permissive-")
endif()
_asc_cpp_add_m8_compile_observation(
  configured
  "${CMAKE_CXX_COMPILER}"
  "${_m8_configured_compiler_style}"
  "${_m8_configured_compile_flags}"
)
find_program(_m8_gnu_compiler NAMES g++-11 g++)
if(_m8_gnu_compiler
   AND NOT _m8_gnu_compiler STREQUAL CMAKE_CXX_COMPILER)
  _asc_cpp_add_m8_compile_observation(
    gcc
    "${_m8_gnu_compiler}"
    gnu
    "-Wall;-Wextra;-Wpedantic;-Werror"
  )
endif()
find_program(_m8_clang_compiler NAMES clang++-19 clang++-18 clang++)
if(_m8_clang_compiler
   AND NOT _m8_clang_compiler STREQUAL CMAKE_CXX_COMPILER)
  _asc_cpp_add_m8_compile_observation(
    clang
    "${_m8_clang_compiler}"
    gnu
    "-Wall;-Wextra;-Wpedantic;-Werror"
  )
endif()

unset(_m8_clang_compiler)
unset(_m8_copied_build_package)
unset(_m8_configured_compiler_style)
unset(_m8_configured_compile_flags)
unset(_m8_cxxfilt)
unset(_m8_cxxfilt_identity)
unset(_m8_distribution)
unset(_m8_elf_baseline)
unset(_m8_fixture_root)
unset(_m8_gnu_compiler)
unset(_m8_install_command)
unset(_m8_install_include)
unset(_m8_install_includedir)
unset(_m8_install_libdir)
unset(_m8_install_package)
unset(_m8_install_prefix)
unset(_m8_binary_inside_source)
unset(_m8_build_hash)
unset(_m8_hardening_report_dir)
unset(_m8_nm)
unset(_m8_nm_identity)
unset(_m8_observation_build_mode)
unset(_m8_relocated_include)
unset(_m8_relocated_package)
unset(_m8_relocated_prefix)
unset(_m8_readelf)
unset(_m8_readelf_identity)
unset(_m8_standard_library)
unset(_m8_tool)
unset(_m8_tool_identity)
unset(_m8_tool_version)
unset(_m8_shared_libraries)
unset(_m8_shared_symbol_command)
unset(_m8_temporary_root)
unset(_m8_temporary_variable)
unset(_m8_baseline_gnu_compiler)
unset(_m8_unlike_cuda_host_compiler)
