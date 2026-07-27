include("${PROJECT_SOURCE_DIR}/tools/hardening/TargetInventory.cmake")
asc_cpp_hardening_check_target_inventory(
  BASELINE "${PROJECT_SOURCE_DIR}/abi/targets-and-components.txt"
  HEADER_OWNERS "${PROJECT_SOURCE_DIR}/abi/header-owners.txt"
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/target-inventory.txt"
)

set(_m8_generator_arguments -G "${CMAKE_GENERATOR}")
if(CMAKE_GENERATOR_PLATFORM)
  list(APPEND _m8_generator_arguments -A "${CMAKE_GENERATOR_PLATFORM}")
endif()
if(CMAKE_GENERATOR_TOOLSET)
  list(APPEND _m8_generator_arguments -T "${CMAKE_GENERATOR_TOOLSET}")
endif()
if(CMAKE_MAKE_PROGRAM)
  list(APPEND _m8_generator_arguments
    "-DCMAKE_MAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}"
  )
endif()

set(_m8_copied_build_package
  "${CMAKE_CURRENT_BINARY_DIR}/copied build package with spaces/ASCCpp"
)
set(_m8_install_prefix
  "${CMAKE_CURRENT_BINARY_DIR}/installed prefix with spaces"
)
set(_m8_relocated_prefix
  "${CMAKE_CURRENT_BINARY_DIR}/relocated prefix with spaces"
)
set(_m8_install_package
  "${_m8_install_prefix}/lib/cmake/ASCCpp"
)
set(_m8_relocated_package
  "${_m8_relocated_prefix}/lib/cmake/ASCCpp"
)

set(ASC_CPP_M8_COPIED_BUILD_PACKAGE
  "${_m8_copied_build_package}"
  CACHE INTERNAL "M8 copied build-tree package used by downstream tests"
)
set(ASC_CPP_M8_INSTALL_PACKAGE
  "${_m8_install_package}"
  CACHE INTERNAL "M8 installed package used by downstream tests"
)
set(ASC_CPP_M8_RELOCATED_PACKAGE
  "${_m8_relocated_package}"
  CACHE INTERNAL "M8 relocated package used by downstream tests"
)

add_test(
  NAME asc_cpp.hardening.public_surface.source
  COMMAND
    "${CMAKE_COMMAND}"
    "-DASC_CPP_HARDENING_SOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    "-DASC_CPP_HARDENING_OUTPUT:FILEPATH=${CMAKE_CURRENT_BINARY_DIR}/public-surface-source.txt"
    -P "${PROJECT_SOURCE_DIR}/tools/hardening/CheckPublicSurface.cmake"
)
set_tests_properties(
  asc_cpp.hardening.public_surface.source
  PROPERTIES LABELS "hardening;api;package;milestone-8"
)

add_test(
  NAME asc_cpp.hardening.copy_build_package
  COMMAND
    "${CMAKE_COMMAND}" -E copy_directory
    "${ASC_CPP_BUILD_PACKAGE_DIR}"
    "${_m8_copied_build_package}"
)
set_tests_properties(
  asc_cpp.hardening.copy_build_package
  PROPERTIES
    FIXTURES_SETUP M8CopiedBuildPackage
    LABELS "hardening;package;milestone-8"
)

add_test(
  NAME asc_cpp.hardening.install_package
  COMMAND
    "${CMAKE_COMMAND}" --install "${PROJECT_BINARY_DIR}"
    --prefix "${_m8_install_prefix}"
    --config "$<CONFIG>"
)
set_tests_properties(
  asc_cpp.hardening.install_package
  PROPERTIES
    FIXTURES_SETUP M8InstalledPackage
    LABELS "hardening;package;milestone-8"
    RUN_SERIAL TRUE
    TIMEOUT 180
)

add_test(
  NAME asc_cpp.hardening.relocate_package
  COMMAND
    "${CMAKE_COMMAND}" -E copy_directory
    "${_m8_install_prefix}"
    "${_m8_relocated_prefix}"
)
set_tests_properties(
  asc_cpp.hardening.relocate_package
  PROPERTIES
    FIXTURES_REQUIRED M8InstalledPackage
    FIXTURES_SETUP M8RelocatedPackage
    LABELS "hardening;package;relocation;milestone-8"
    RUN_SERIAL TRUE
)

function(_asc_cpp_add_m8_metadata_test
    name package_directory version exact expect_found fixture)
  set(_command
    "${CMAKE_COMMAND}"
    -S "${CMAKE_CURRENT_SOURCE_DIR}/package_metadata"
    -B "${CMAKE_CURRENT_BINARY_DIR}/${name} build"
    ${_m8_generator_arguments}
    "-DASCCpp_DIR:PATH=${package_directory}"
    "-DASCCPP_REQUEST_VERSION:STRING=${version}"
    "-DASCCPP_REQUEST_EXACT:BOOL=${exact}"
    "-DASCCPP_EXPECT_FOUND:BOOL=${expect_found}"
    "-DASCCPP_EXPECT_CUDA:BOOL=${ASC_CPP_ENABLE_CUDA}"
    "-DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY:BOOL=ON"
    "-DCMAKE_FIND_USE_PACKAGE_REGISTRY:BOOL=OFF"
    "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY:BOOL=OFF"
  )
  add_test(
    NAME "asc_cpp.hardening.package_metadata.${name}"
    COMMAND ${_command}
  )
  set_tests_properties(
    "asc_cpp.hardening.package_metadata.${name}"
    PROPERTIES
      LABELS "hardening;package;consumer;milestone-8"
      RUN_SERIAL TRUE
      TIMEOUT 120
  )
  if(fixture)
    set_tests_properties(
      "asc_cpp.hardening.package_metadata.${name}"
      PROPERTIES FIXTURES_REQUIRED "${fixture}"
    )
  endif()
endfunction()

foreach(_mode IN ITEMS build_tree installed)
  if(_mode STREQUAL "build_tree")
    set(_package_directory "${ASC_CPP_BUILD_PACKAGE_DIR}")
    set(_fixture)
  else()
    set(_package_directory "${_m8_relocated_package}")
    set(_fixture M8RelocatedPackage)
  endif()
  _asc_cpp_add_m8_metadata_test(
    "${_mode}.exact_0_9_0" "${_package_directory}" 0.9.0 ON ON "${_fixture}"
  )
  _asc_cpp_add_m8_metadata_test(
    "${_mode}.compatible_0_9" "${_package_directory}" 0.9 OFF ON "${_fixture}"
  )
  _asc_cpp_add_m8_metadata_test(
    "${_mode}.newer_0_9_1" "${_package_directory}" 0.9.1 OFF OFF "${_fixture}"
  )
  _asc_cpp_add_m8_metadata_test(
    "${_mode}.minor_0_8" "${_package_directory}" 0.8 OFF OFF "${_fixture}"
  )
  _asc_cpp_add_m8_metadata_test(
    "${_mode}.major_1_0" "${_package_directory}" 1.0 OFF OFF "${_fixture}"
  )
endforeach()
_asc_cpp_add_m8_metadata_test(
  "copied_build_tree.exact_0_9_0"
  "${_m8_copied_build_package}"
  0.9.0
  ON
  ON
  M8CopiedBuildPackage
)

add_test(
  NAME asc_cpp.hardening.installed_headers
  COMMAND
    "${CMAKE_COMMAND}"
    "-DINCLUDE_ROOT:PATH=${_m8_relocated_prefix}/include"
    "-DENABLE_CUDA:BOOL=${ASC_CPP_ENABLE_CUDA}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_installed_headers.cmake"
)
set_tests_properties(
  asc_cpp.hardening.installed_headers
  PROPERTIES
    FIXTURES_REQUIRED M8RelocatedPackage
    LABELS "hardening;package;api;milestone-8"
    RUN_SERIAL TRUE
)

add_test(
  NAME asc_cpp.hardening.public_surface.installed
  COMMAND
    "${CMAKE_COMMAND}"
    "-DASC_CPP_HARDENING_SOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    "-DASC_CPP_HARDENING_INSTALLED_INCLUDE_DIR:PATH=${_m8_relocated_prefix}/include"
    "-DASC_CPP_HARDENING_OUTPUT:FILEPATH=${CMAKE_CURRENT_BINARY_DIR}/public-surface-installed.txt"
    -P "${PROJECT_SOURCE_DIR}/tools/hardening/CheckPublicSurface.cmake"
)
set_tests_properties(
  asc_cpp.hardening.public_surface.installed
  PROPERTIES
    FIXTURES_REQUIRED M8RelocatedPackage
    LABELS "hardening;api;package;milestone-8"
    RUN_SERIAL TRUE
)

if(BUILD_SHARED_LIBS)
  set(_m8_shared_libraries
    "$<TARGET_FILE:asc_core>"
    "$<TARGET_FILE:asc_utilities>"
    "$<TARGET_FILE:asc_random>"
    "$<TARGET_FILE:asc_dense>"
    "$<TARGET_FILE:asc_sparse>"
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

  set(_m8_elf_baseline)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
     AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 11
     AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12
     AND NOT ASC_CPP_ENABLE_CUDA
     AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_m8_elf_baseline
      "${PROJECT_SOURCE_DIR}/abi/linux-x86_64-gcc11-cpu-shared.txt"
    )
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang"
         AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19
         AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 20
         AND NOT ASC_CPP_ENABLE_CUDA
         AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_m8_elf_baseline
      "${PROJECT_SOURCE_DIR}/abi/linux-x86_64-clang19-cpu-shared.txt"
    )
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
         AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 11
         AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12
         AND ASC_CPP_ENABLE_CUDA
         AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set(_m8_elf_baseline
      "${PROJECT_SOURCE_DIR}/abi/linux-x86_64-gcc11-cuda12-shared.txt"
    )
  endif()

  add_test(
    NAME asc_cpp.hardening.elf_abi
    COMMAND
      "${CMAKE_COMMAND}"
      "-DASC_CPP_HARDENING_SOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
      "-DASC_CPP_HARDENING_LIBRARIES:STRING=${_m8_shared_libraries}"
      "-DASC_CPP_HARDENING_OUTPUT:FILEPATH=${CMAKE_CURRENT_BINARY_DIR}/elf-abi-$<CONFIG>.txt"
      "-DASC_CPP_HARDENING_BASELINE:FILEPATH=${_m8_elf_baseline}"
      -P "${PROJECT_SOURCE_DIR}/tools/hardening/InspectElfAbi.cmake"
  )
  set_tests_properties(
    asc_cpp.hardening.elf_abi
    PROPERTIES
      LABELS "hardening;api;abi;package;milestone-8"
      RUN_SERIAL TRUE
      SKIP_REGULAR_EXPRESSION "ASCCpp ELF ABI inspection skipped"
      TIMEOUT 120
  )
endif()

set(_m8_installed_api_build
  "${CMAKE_CURRENT_BINARY_DIR}/installed api build with spaces"
)
add_test(
  NAME asc_cpp.hardening.installed_api.configure
  COMMAND
    "${CMAKE_COMMAND}"
    -S "${CMAKE_CURRENT_SOURCE_DIR}/installed_api"
    -B "${_m8_installed_api_build}"
    ${_m8_generator_arguments}
    "-DASCCpp_DIR:PATH=${_m8_relocated_package}"
    "-DASCCPP_EXPECT_CUDA:BOOL=${ASC_CPP_ENABLE_CUDA}"
    "-DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY:BOOL=ON"
    "-DCMAKE_FIND_USE_PACKAGE_REGISTRY:BOOL=OFF"
    "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY:BOOL=OFF"
)
add_test(
  NAME asc_cpp.hardening.installed_api.build
  COMMAND
    "${CMAKE_COMMAND}" --build "${_m8_installed_api_build}"
    --config "$<CONFIG>"
)
add_test(
  NAME asc_cpp.hardening.installed_api.run
  COMMAND
    "${CMAKE_CTEST_COMMAND}"
    --test-dir "${_m8_installed_api_build}"
    -C "$<CONFIG>"
    --output-on-failure
    --no-tests=error
)
set_tests_properties(
  asc_cpp.hardening.installed_api.configure
  PROPERTIES
    FIXTURES_REQUIRED M8RelocatedPackage
    LABELS "hardening;package;api;consumer;milestone-8"
    RUN_SERIAL TRUE
    TIMEOUT 120
)
set_tests_properties(
  asc_cpp.hardening.installed_api.build
  PROPERTIES
    DEPENDS asc_cpp.hardening.installed_api.configure
    LABELS "hardening;package;api;consumer;milestone-8"
    RUN_SERIAL TRUE
    TIMEOUT 180
)
set_tests_properties(
  asc_cpp.hardening.installed_api.run
  PROPERTIES
    DEPENDS asc_cpp.hardening.installed_api.build
    LABELS "hardening;package;api;consumer;milestone-8"
    RUN_SERIAL TRUE
    TIMEOUT 120
)

function(_asc_cpp_add_compile_object_probe name compiler compiler_id)
  add_test(
    NAME "asc_cpp.hardening.compile_object.${name}"
    COMMAND
      "${CMAKE_COMMAND}"
      "-DCXX_COMPILER:FILEPATH=${compiler}"
      "-DCOMPILER_ID:STRING=${compiler_id}"
      "-DINCLUDE_DIR:PATH=${PROJECT_SOURCE_DIR}/include"
      "-DSOURCE_FILE:FILEPATH=${PROJECT_SOURCE_DIR}/benchmarks/hardening/public_header_compile_probe.cc"
      "-DOUTPUT_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}/compile object ${name}"
      "-DREPETITIONS:STRING=3"
      -P "${PROJECT_SOURCE_DIR}/benchmarks/hardening/run_compile_object_probe.cmake"
  )
  set_tests_properties(
    "asc_cpp.hardening.compile_object.${name}"
    PROPERTIES
      LABELS "hardening;performance;compile;milestone-8"
      RUN_SERIAL TRUE
      TIMEOUT 120
  )
endfunction()

find_program(_m8_gnu_compiler NAMES g++-11 g++)
if(_m8_gnu_compiler)
  _asc_cpp_add_compile_object_probe(gcc "${_m8_gnu_compiler}" GNU)
endif()
find_program(_m8_clang_compiler NAMES clang++-19 clang++-18 clang++)
if(_m8_clang_compiler)
  _asc_cpp_add_compile_object_probe(clang "${_m8_clang_compiler}" Clang)
endif()

unset(_fixture)
unset(_m8_clang_compiler)
unset(_m8_copied_build_package)
unset(_m8_generator_arguments)
unset(_m8_gnu_compiler)
unset(_m8_install_package)
unset(_m8_install_prefix)
unset(_m8_installed_api_build)
unset(_m8_elf_baseline)
unset(_m8_relocated_package)
unset(_m8_relocated_prefix)
unset(_m8_shared_libraries)
unset(_mode)
unset(_package_directory)
