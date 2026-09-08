function(_asc_dense_lapack_configure_test target)
  target_link_libraries("${target}" PRIVATE ASC::dense_lapack)
  asc_target_enable_cxx20(TARGET "${target}")
  if(ASC_CPP_WARNINGS_AS_ERRORS)
    asc_target_enable_warnings(TARGET "${target}" AS_ERRORS)
  else()
    asc_target_enable_warnings(TARGET "${target}")
  endif()
  if(ASC_CPP_SANITIZER_ARGUMENTS)
    asc_target_enable_sanitizers(TARGET "${target}" ${ASC_CPP_SANITIZER_ARGUMENTS})
  endif()
endfunction()

add_executable(asc_dense_lapack_normal_return_probe normal_return_probe.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_normal_return_probe)
target_include_directories(asc_dense_lapack_normal_return_probe PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
add_test(NAME asc_cpp.dense_lapack.normal_return_guard
  COMMAND "${CMAKE_COMMAND}"
    "-DPROBE:FILEPATH=$<TARGET_FILE:asc_dense_lapack_normal_return_probe>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/normal_return_guard_test.cmake")
set_tests_properties(asc_cpp.dense_lapack.normal_return_guard PROPERTIES
  LABELS "dense_lapack;lapack;runtime;validation" TIMEOUT 40)

add_executable(asc_dense_lapack_reference_lu_test
  reference_lu_test.cc allocation_audit.cc fault_injection.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_reference_lu_test)
target_include_directories(asc_dense_lapack_reference_lu_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_reference_lu_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free,--wrap=dgetrf_")
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.reference_lu.${_scalar}"
    TARGET asc_dense_lapack_reference_lu_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack runtime numerical allocation TIMEOUT 120)
endforeach()

add_executable(asc_dense_lapack_lu_info_test
  lu_info_test.cc lu_info_faults.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_lu_info_test)
target_include_directories(asc_dense_lapack_lu_info_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_lu_info_test PRIVATE
  "-Wl,--wrap=sgetrf_,--wrap=dgetrf_,--wrap=cgetrf_,--wrap=zgetrf_,--wrap=sgetrs_,--wrap=dgetrs_,--wrap=cgetrs_,--wrap=zgetrs_"
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.lu_info.${_scalar}"
    TARGET asc_dense_lapack_lu_info_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack runtime numerical validation allocation
    TIMEOUT 120)
endforeach()

add_executable(asc_dense_lapack_lu_expert_info_test
  lu_expert_info_test.cc lu_expert_info_faults.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_lu_expert_info_test)
target_include_directories(asc_dense_lapack_lu_expert_info_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_lu_expert_info_test PRIVATE
  "-Wl,--wrap=sgetrf2_,--wrap=sgetf2_,--wrap=sgetri_,--wrap=sgesv_,--wrap=dgetrf2_,--wrap=dgetf2_,--wrap=dgetri_,--wrap=dgesv_,--wrap=cgetrf2_,--wrap=cgetf2_,--wrap=cgetri_,--wrap=cgesv_,--wrap=zgetrf2_,--wrap=zgetf2_,--wrap=zgetri_,--wrap=zgesv_"
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.lu_expert_info.${_scalar}"
    TARGET asc_dense_lapack_lu_expert_info_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack runtime numerical validation allocation
    TIMEOUT 120)
endforeach()

add_executable(asc_dense_lapack_lu_pivot_test
  lu_pivot_test.cc lu_pivot_faults.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_lu_pivot_test)
target_include_directories(asc_dense_lapack_lu_pivot_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}"
  "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_lu_pivot_test PRIVATE
  "-Wl,--wrap=sgetrf_,--wrap=sgetrf2_,--wrap=sgetf2_,--wrap=sgesv_,--wrap=dgetrf_,--wrap=dgetrf2_,--wrap=dgetf2_,--wrap=dgesv_,--wrap=cgetrf_,--wrap=cgetrf2_,--wrap=cgetf2_,--wrap=cgesv_,--wrap=zgetrf_,--wrap=zgetrf2_,--wrap=zgetf2_,--wrap=zgesv_"
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.lu_pivot.${_scalar}"
    TARGET asc_dense_lapack_lu_pivot_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack runtime numerical validation allocation
    TIMEOUT 120)
endforeach()

add_executable(asc_dense_lapack_lu_layout_test
  lu_layout_test.cc allocation_audit.cc fault_injection.cc lu_expert_faults.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_lu_layout_test)
target_include_directories(asc_dense_lapack_lu_layout_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}"
  "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_lu_layout_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free,--wrap=dgetrf_,--wrap=dgetrf2_,--wrap=dgetf2_,--wrap=dgetri_,--wrap=dgesv_")
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.lu_layout.${_scalar}"
    TARGET asc_dense_lapack_lu_layout_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack runtime numerical allocation TIMEOUT 120)
endforeach()

foreach(_family IN ITEMS expert equilibration condition refinement driver helpers)
  set(_target "asc_dense_lapack_lu_${_family}_test")
  add_executable("${_target}"
    "lu_${_family}_test.cc" "lu_${_family}_faults.cc" allocation_audit.cc
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test("${_target}")
  target_include_directories("${_target}" PRIVATE
    "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}"
    "${ASC_CPP_LAPACK_ROOT}/include")
  target_link_options("${_target}" PRIVATE
    "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
  if(_family STREQUAL "expert")
    target_link_options("${_target}" PRIVATE
      "-Wl,--wrap=dgetrf2_,--wrap=dgetf2_,--wrap=dgetri_,--wrap=dgesv_")
  elseif(_family STREQUAL "equilibration")
    target_link_options("${_target}" PRIVATE "-Wl,--wrap=dgeequ_,--wrap=dgeequb_")
  elseif(_family STREQUAL "condition")
    target_link_options("${_target}" PRIVATE "-Wl,--wrap=dgecon_")
  elseif(_family STREQUAL "refinement")
    target_link_options("${_target}" PRIVATE "-Wl,--wrap=dgerfs_")
  elseif(_family STREQUAL "driver")
    target_link_options("${_target}" PRIVATE "-Wl,--wrap=dgesvx_")
  else()
    target_link_options("${_target}" PRIVATE "-Wl,--wrap=dlaswp_,--wrap=dlaqge_")
  endif()
  foreach(_scalar IN ITEMS s d c z)
    asc_register_test(NAME "asc_cpp.dense_lapack.lu_${_family}.${_scalar}"
      TARGET "${_target}" ARGUMENTS "${_scalar}"
      LABELS dense_lapack lapack runtime numerical allocation TIMEOUT 120)
  endforeach()
endforeach()

add_executable(asc_dense_lapack_provider_placement_test
  provider_placement_test.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_provider_placement_test)
target_include_directories(asc_dense_lapack_provider_placement_test PRIVATE
  "${PROJECT_SOURCE_DIR}")
target_link_options(asc_dense_lapack_provider_placement_test PRIVATE
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.provider_placement.${_scalar}"
    TARGET asc_dense_lapack_provider_placement_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack runtime validation allocation TIMEOUT 120)
endforeach()

foreach(_family IN ITEMS lu qr)
  set(_target "asc_dense_lapack_${_family}_counts_test")
  add_executable("${_target}" "${_family}_counts_test.cc"
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test("${_target}")
  target_include_directories("${_target}" PRIVATE "${PROJECT_SOURCE_DIR}")
  asc_register_test(NAME "asc_cpp.dense_lapack.${_family}_counts"
    TARGET "${_target}" LABELS dense_lapack lapack runtime validation allocation)
endforeach()

add_executable(asc_dense_lapack_reference_cholesky_test
  reference_cholesky_test.cc cholesky_faults.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_reference_cholesky_test)
target_include_directories(asc_dense_lapack_reference_cholesky_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}"
  "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_reference_cholesky_test PRIVATE
  "-Wl,--wrap=dpotrf_,--wrap=dpotrf2_,--wrap=dpotf2_,--wrap=dpotrs_,--wrap=dpotri_,--wrap=dposv_")
asc_register_test(NAME asc_cpp.dense_lapack.reference_cholesky
  TARGET asc_dense_lapack_reference_cholesky_test
  LABELS dense_lapack lapack runtime numerical allocation TIMEOUT 120)

add_executable(asc_dense_lapack_cholesky_stride_test cholesky_stride_test.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_cholesky_stride_test)
asc_register_test(NAME asc_cpp.dense_lapack.cholesky_stride
  TARGET asc_dense_lapack_cholesky_stride_test
  LABELS dense_lapack lapack runtime validation numerical)

foreach(_test IN ITEMS reference_qr qr_workspace_placement)
  set(_target "asc_dense_lapack_${_test}_test")
  add_executable("${_target}" "${_test}_test.cc" qr_faults.cc
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test("${_target}")
  target_include_directories("${_target}" PRIVATE
    "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}"
    "${ASC_CPP_LAPACK_ROOT}/include")
  foreach(_symbol IN ITEMS
      sgeqrf dgeqrf cgeqrf zgeqrf sgeqr2 dgeqr2 cgeqr2 zgeqr2
      sorgqr dorgqr cungqr zungqr sormqr dormqr cunmqr zunmqr)
    target_link_options("${_target}" PRIVATE "-Wl,--wrap=${_symbol}_")
  endforeach()
  if(_test STREQUAL "reference_qr")
    foreach(_scalar IN ITEMS s d c z)
      asc_register_test(NAME "asc_cpp.dense_lapack.reference_qr.${_scalar}"
        TARGET "${_target}" ARGUMENTS "${_scalar}"
        LABELS dense_lapack lapack runtime numerical allocation TIMEOUT 120)
    endforeach()
  else()
    asc_register_test(NAME asc_cpp.dense_lapack.qr_workspace_placement
      TARGET "${_target}" LABELS dense_lapack lapack runtime validation allocation)
  endif()
endforeach()

foreach(_suite IN ITEMS diagonal solve defensive boundary)
  set(_target "asc_dense_lapack_cholesky_expert_${_suite}_test")
  add_executable("${_target}" "cholesky_expert_${_suite}_test.cc"
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test("${_target}")
  target_include_directories("${_target}" PRIVATE "${PROJECT_SOURCE_DIR}")
  if(_suite STREQUAL "defensive")
    target_sources("${_target}" PRIVATE cholesky_expert_faults.cc)
    target_include_directories("${_target}" PRIVATE
      "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
    foreach(_symbol IN ITEMS dpocon dporfs dposvx dpoequ dpoequb)
      target_link_options("${_target}" PRIVATE "-Wl,--wrap=${_symbol}_")
    endforeach()
  endif()
  asc_register_test(NAME "asc_cpp.dense_lapack.cholesky_expert.${_suite}"
    TARGET "${_target}" LABELS dense_lapack lapack runtime numerical allocation
    TIMEOUT 120)
endforeach()

foreach(_suite IN ITEMS reference_least_squares least_squares_contract
    least_squares_workspace least_squares_counts)
  set(_target "asc_dense_lapack_${_suite}_test")
  add_executable("${_target}" "${_suite}_test.cc"
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test("${_target}")
  target_include_directories("${_target}" PRIVATE "${PROJECT_SOURCE_DIR}")
  if(NOT _suite STREQUAL "least_squares_counts")
    target_sources("${_target}" PRIVATE least_squares_faults.cc)
    target_include_directories("${_target}" PRIVATE
      "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
    foreach(_symbol IN ITEMS sgels dgels cgels zgels sgelst dgelst cgelst zgelst
        sgetsls dgetsls cgetsls zgetsls)
      target_link_options("${_target}" PRIVATE "-Wl,--wrap=${_symbol}_")
    endforeach()
  endif()
  if(_suite STREQUAL "reference_least_squares")
    foreach(_scalar IN ITEMS s d c z)
      asc_register_test(NAME "asc_cpp.dense_lapack.reference_least_squares.${_scalar}"
        TARGET "${_target}" ARGUMENTS "${_scalar}"
        LABELS dense_lapack lapack runtime numerical allocation TIMEOUT 120)
    endforeach()
  else()
    asc_register_test(NAME "asc_cpp.dense_lapack.${_suite}"
      TARGET "${_target}" LABELS dense_lapack lapack runtime validation allocation
      TIMEOUT 120)
  endif()
endforeach()

foreach(_suite IN ITEMS band_cholesky band_cholesky_fidelity
    band_cholesky_validation band_cholesky_limits)
  set(_target "asc_dense_lapack_${_suite}_test")
  add_executable("${_target}" "${_suite}_test.cc"
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test("${_target}")
  target_include_directories("${_target}" PRIVATE "${PROJECT_SOURCE_DIR}")
  if(NOT _suite STREQUAL "band_cholesky_limits")
    target_sources("${_target}" PRIVATE allocation_audit.cc)
    target_link_options("${_target}" PRIVATE
      "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
  endif()
  if(_suite STREQUAL "band_cholesky_fidelity" OR
      _suite STREQUAL "band_cholesky_validation")
    target_include_directories("${_target}" PRIVATE
      "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  endif()
  if(_suite STREQUAL "band_cholesky_validation")
    target_sources("${_target}" PRIVATE band_cholesky_fault_support.cc)
    foreach(_scalar IN ITEMS s d c z)
      foreach(_routine IN ITEMS pbtrf pbtf2 pbtrs)
        target_link_options("${_target}" PRIVATE "-Wl,--wrap=${_scalar}${_routine}_")
      endforeach()
    endforeach()
  endif()
  if(_suite STREQUAL "band_cholesky_limits")
    asc_register_test(NAME "asc_cpp.dense_lapack.${_suite}"
      TARGET "${_target}" LABELS dense_lapack lapack validation TIMEOUT 120)
  else()
    foreach(_scalar IN ITEMS s d c z)
      asc_register_test(NAME "asc_cpp.dense_lapack.${_suite}.${_scalar}"
        TARGET "${_target}" ARGUMENTS "${_scalar}"
        LABELS dense_lapack lapack runtime numerical allocation TIMEOUT 120)
    endforeach()
  endif()
endforeach()

foreach(_suite IN ITEMS indefinite indefinite_preflight indefinite_counts
    indefinite_fault)
  set(_target "asc_dense_lapack_${_suite}_test")
  add_executable("${_target}" "${_suite}_test.cc"
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test("${_target}")
  target_include_directories("${_target}" PRIVATE "${PROJECT_SOURCE_DIR}")
  if(NOT _suite STREQUAL "indefinite_counts")
    target_sources("${_target}" PRIVATE allocation_audit.cc)
    target_link_options("${_target}" PRIVATE
      "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
  endif()
  if(_suite STREQUAL "indefinite_fault")
    target_sources("${_target}" PRIVATE indefinite_faults.cc)
    target_include_directories("${_target}" PRIVATE
      "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
    foreach(_symbol IN ITEMS dsytrf dsytf2 dsytrs)
      target_link_options("${_target}" PRIVATE "-Wl,--wrap=${_symbol}_")
    endforeach()
  endif()
  if(_suite STREQUAL "indefinite" OR _suite STREQUAL "indefinite_preflight")
    foreach(_scalar IN ITEMS s d c z ch zh)
      asc_register_test(NAME "asc_cpp.dense_lapack.${_suite}.${_scalar}"
        TARGET "${_target}" ARGUMENTS "${_scalar}"
        LABELS dense_lapack lapack runtime numerical allocation TIMEOUT 120)
    endforeach()
  else()
    asc_register_test(NAME "asc_cpp.dense_lapack.${_suite}"
      TARGET "${_target}" LABELS dense_lapack lapack validation allocation
      TIMEOUT 120)
  endif()
endforeach()

add_executable(asc_dense_lapack_indefinite_abi_probe indefinite_abi_probe.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_abi_probe)
target_include_directories(asc_dense_lapack_indefinite_abi_probe PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}"
  "${ASC_CPP_LAPACK_ROOT}/include")
# Exact compiler-emitted type comparisons are retained in the separately
# audited external ABI lane. This installed-header route always executes all
# six real TF2 symbols with full-width argument/pivot/INFO sentinels.
asc_register_test(NAME asc_cpp.dense_lapack.indefinite_abi_probe
  TARGET asc_dense_lapack_indefinite_abi_probe
  LABELS dense_lapack lapack abi runtime TIMEOUT 120)

foreach(_suite IN ITEMS reference_rank_revealing rank_revealing_contract
    rank_revealing_alias rank_revealing_empty rank_revealing_counts)
  set(_target "asc_dense_lapack_${_suite}_test")
  add_executable("${_target}" "${_suite}_test.cc"
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test("${_target}")
  target_include_directories("${_target}" PRIVATE
    "${PROJECT_SOURCE_DIR}/src/dense/lapack"
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  if(NOT _suite STREQUAL "rank_revealing_counts")
    target_sources("${_target}" PRIVATE rank_revealing_faults.cc allocation_audit.cc)
    target_link_options("${_target}" PRIVATE
      "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
    foreach(_scalar IN ITEMS s d c z)
      foreach(_routine IN ITEMS geqp3 gelsy)
        target_link_options("${_target}" PRIVATE "-Wl,--wrap=${_scalar}${_routine}_")
      endforeach()
    endforeach()
  endif()
  if(_suite STREQUAL "reference_rank_revealing")
    foreach(_scalar IN ITEMS s d c z)
      asc_register_test(NAME "asc_cpp.dense_lapack.rank_revealing.${_scalar}"
        TARGET "${_target}" ARGUMENTS "${_scalar}"
        LABELS dense_lapack lapack runtime numerical allocation TIMEOUT 120)
      foreach(_routine IN ITEMS geqp3 gelsy)
        asc_register_test(NAME "asc_cpp.dense_lapack.rank_cold.${_routine}.${_scalar}"
          TARGET "${_target}" ARGUMENTS "${_scalar}" "${_routine}"
          LABELS dense_lapack lapack runtime numerical allocation TIMEOUT 120)
      endforeach()
    endforeach()
  else()
    asc_register_test(NAME "asc_cpp.dense_lapack.${_suite}"
      TARGET "${_target}" LABELS dense_lapack lapack runtime validation allocation
      TIMEOUT 120)
  endif()
endforeach()

foreach(_suite IN ITEMS reference_sylvester sylvester_contract
    sylvester_alias sylvester_counts)
  set(_target "asc_dense_lapack_${_suite}_test")
  add_executable("${_target}" "${_suite}_test.cc"
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test("${_target}")
  target_include_directories("${_target}" PRIVATE
    "${PROJECT_SOURCE_DIR}/src/dense/lapack"
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  if(_suite STREQUAL "sylvester_counts")
    asc_register_test(NAME "asc_cpp.dense_lapack.${_suite}"
      TARGET "${_target}" LABELS dense_lapack lapack validation allocation
      TIMEOUT 120)
  else()
    target_sources("${_target}" PRIVATE sylvester_faults.cc allocation_audit.cc)
    target_link_options("${_target}" PRIVATE
      "-Wl,--wrap=strsyl_,--wrap=dtrsyl_,--wrap=ctrsyl_,--wrap=ztrsyl_"
      "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
    foreach(_scalar IN ITEMS s d c z)
      asc_register_test(NAME "asc_cpp.dense_lapack.${_suite}.${_scalar}"
        TARGET "${_target}" ARGUMENTS "${_scalar}"
        LABELS dense_lapack lapack runtime numerical validation allocation
        TIMEOUT 120)
    endforeach()
  endif()
endforeach()
add_executable(asc_dense_lapack_sylvester_abi_probe sylvester_abi_probe.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_sylvester_abi_probe)
target_include_directories(asc_dense_lapack_sylvester_abi_probe PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME asc_cpp.dense_lapack.sylvester_abi
  TARGET asc_dense_lapack_sylvester_abi_probe
  LABELS dense_lapack lapack runtime abi TIMEOUT 120)

foreach(_suite IN ITEMS indefinite_condition indefinite_driver
    indefinite_refinement indefinite_expert_driver indefinite_expert_fault)
  set(_target "asc_dense_lapack_${_suite}_test")
  add_executable("${_target}" "${_suite}_test.cc" allocation_audit.cc
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test("${_target}")
  target_include_directories("${_target}" PRIVATE
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  target_link_options("${_target}" PRIVATE
    "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
  if(_suite STREQUAL "indefinite_expert_fault")
    target_sources("${_target}" PRIVATE indefinite_expert_faults.cc)
    foreach(_scalar IN ITEMS s d c z)
      foreach(_routine IN ITEMS sycon syrfs sysv sysvx)
        target_link_options("${_target}" PRIVATE
          "-Wl,--wrap=${_scalar}${_routine}_")
      endforeach()
    endforeach()
    foreach(_scalar IN ITEMS c z)
      foreach(_routine IN ITEMS hecon herfs hesv hesvx)
        target_link_options("${_target}" PRIVATE
          "-Wl,--wrap=${_scalar}${_routine}_")
      endforeach()
    endforeach()
  endif()
  foreach(_scalar IN ITEMS s d c z ch zh)
    asc_register_test(NAME "asc_cpp.dense_lapack.${_suite}.${_scalar}"
      TARGET "${_target}" ARGUMENTS "${_scalar}"
      LABELS dense_lapack lapack runtime numerical validation allocation
      TIMEOUT 120)
  endforeach()
endforeach()
add_executable(asc_dense_lapack_indefinite_expert_counts_test
  indefinite_expert_counts_test.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_expert_counts_test)
asc_register_test(NAME asc_cpp.dense_lapack.indefinite_expert_counts
  TARGET asc_dense_lapack_indefinite_expert_counts_test
  LABELS dense_lapack lapack validation TIMEOUT 120)
add_executable(asc_dense_lapack_indefinite_expert_abi_probe
  indefinite_expert_abi_probe.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_indefinite_expert_abi_probe)
target_include_directories(asc_dense_lapack_indefinite_expert_abi_probe PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME asc_cpp.dense_lapack.indefinite_expert_abi
  TARGET asc_dense_lapack_indefinite_expert_abi_probe
  LABELS dense_lapack lapack runtime abi TIMEOUT 120)

foreach(_suite IN ITEMS reference_svd_least_squares svd_least_squares_contract
    svd_least_squares_fault svd_least_squares_counts)
  set(_target "asc_dense_lapack_${_suite}_test")
  add_executable("${_target}" "${_suite}_test.cc"
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test("${_target}")
  target_include_directories("${_target}" PRIVATE
    "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/src/dense/lapack"
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  if(_suite STREQUAL "svd_least_squares_counts")
    asc_register_test(NAME "asc_cpp.dense_lapack.${_suite}"
      TARGET "${_target}" LABELS dense_lapack lapack validation allocation
      TIMEOUT 300)
  else()
    target_sources("${_target}" PRIVATE svd_least_squares_faults.cc)
    foreach(_scalar IN ITEMS s d c z)
      foreach(_routine IN ITEMS gelss gelsd)
        target_link_options("${_target}" PRIVATE
          "-Wl,--wrap=${_scalar}${_routine}_")
      endforeach()
      asc_register_test(NAME "asc_cpp.dense_lapack.${_suite}.${_scalar}"
        TARGET "${_target}" ARGUMENTS "${_scalar}"
        LABELS dense_lapack lapack runtime numerical validation allocation
        TIMEOUT 300)
      if(_suite STREQUAL "reference_svd_least_squares")
        foreach(_routine IN ITEMS gelss gelsd)
          asc_register_test(
            NAME "asc_cpp.dense_lapack.svd_least_squares_cold.${_routine}.${_scalar}"
            TARGET "${_target}" ARGUMENTS "${_scalar}" "${_routine}"
            LABELS dense_lapack lapack runtime numerical allocation TIMEOUT 300)
        endforeach()
      endif()
    endforeach()
  endif()
endforeach()

foreach(_suite IN ITEMS lu_band lu_band_fault lu_band_boundary)
  set(_target "asc_dense_lapack_${_suite}_test")
  add_executable("${_target}" "${_suite}_test.cc" allocation_audit.cc
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test("${_target}")
  target_include_directories("${_target}" PRIVATE
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  target_link_options("${_target}" PRIVATE
    "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
  if(NOT _suite STREQUAL "lu_band")
    target_sources("${_target}" PRIVATE lu_band_faults.cc)
    foreach(_scalar IN ITEMS s d c z)
      foreach(_routine IN ITEMS gbtrf gbtrs)
        target_link_options("${_target}" PRIVATE
          "-Wl,--wrap=${_scalar}${_routine}_")
      endforeach()
    endforeach()
  endif()
  foreach(_scalar IN ITEMS s d c z)
    asc_register_test(NAME "asc_cpp.dense_lapack.${_suite}.${_scalar}"
      TARGET "${_target}" ARGUMENTS "${_scalar}"
      LABELS dense_lapack lapack runtime numerical validation allocation
      TIMEOUT 180)
  endforeach()
endforeach()
add_executable(asc_dense_lapack_lu_band_limits_test lu_band_limits_test.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_lu_band_limits_test)
asc_register_test(NAME asc_cpp.dense_lapack.lu_band_limits
  TARGET asc_dense_lapack_lu_band_limits_test
  LABELS dense_lapack lapack validation TIMEOUT 120)
add_executable(asc_dense_lapack_lu_band_abi_probe lu_band_abi_probe.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_lu_band_abi_probe)
target_include_directories(asc_dense_lapack_lu_band_abi_probe PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME asc_cpp.dense_lapack.lu_band_abi
  TARGET asc_dense_lapack_lu_band_abi_probe
  LABELS dense_lapack lapack runtime abi TIMEOUT 120)

foreach(_suite IN ITEMS tridiagonal tridiagonal_expert tridiagonal_fault
    tridiagonal_extreme_math tridiagonal_counts)
  set(_target "asc_dense_lapack_${_suite}_test")
  add_executable("${_target}" "${_suite}_test.cc")
  _asc_dense_lapack_configure_test("${_target}")
  target_include_directories("${_target}" PRIVATE
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  if(_suite MATCHES "^(tridiagonal|tridiagonal_expert|tridiagonal_fault)$")
    target_sources("${_target}" PRIVATE allocation_audit.cc
      "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
    target_link_options("${_target}" PRIVATE
      "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
  endif()
  if(_suite STREQUAL "tridiagonal_fault")
    target_sources("${_target}" PRIVATE tridiagonal_faults.cc)
    foreach(_scalar IN ITEMS s d c z)
      foreach(_routine IN ITEMS gttrf gttrs gtsv gtcon gtrfs gtsvx)
        target_link_options("${_target}" PRIVATE
          "-Wl,--wrap=${_scalar}${_routine}_")
      endforeach()
    endforeach()
  endif()
  if(_suite STREQUAL "tridiagonal_counts")
    asc_register_test(NAME "asc_cpp.dense_lapack.${_suite}"
      TARGET "${_target}" LABELS dense_lapack lapack validation TIMEOUT 120)
  else()
    foreach(_scalar IN ITEMS s d c z)
      if(_suite STREQUAL "tridiagonal")
        set(_operations factor driver)
      elseif(_suite STREQUAL "tridiagonal_expert")
        set(_operations condition refine expert)
      else()
        set(_operations "")
        asc_register_test(NAME "asc_cpp.dense_lapack.${_suite}.${_scalar}"
          TARGET "${_target}" ARGUMENTS "${_scalar}"
          LABELS dense_lapack lapack runtime numerical validation TIMEOUT 180)
      endif()
      foreach(_operation IN LISTS _operations)
        asc_register_test(
          NAME "asc_cpp.dense_lapack.tridiagonal_${_operation}.${_scalar}"
          TARGET "${_target}" ARGUMENTS "${_scalar}" "${_operation}"
          LABELS dense_lapack lapack runtime numerical allocation TIMEOUT 180)
      endforeach()
      if(_suite STREQUAL "tridiagonal_fault")
        asc_register_test(
          NAME "asc_cpp.dense_lapack.tridiagonal_boundary.${_scalar}"
          TARGET "${_target}" ARGUMENTS "${_scalar}" boundary
          LABELS dense_lapack lapack runtime validation allocation TIMEOUT 180)
      endif()
    endforeach()
  endif()
endforeach()

function(_asc_dense_lapack_add_band_expert_test suite)
  set(_target "asc_dense_lapack_${suite}_test")
  add_executable("${_target}" "${suite}_test.cc" allocation_audit.cc
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test("${_target}")
  target_include_directories("${_target}" PRIVATE
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  target_link_options("${_target}" PRIVATE
    "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
  foreach(_scalar IN ITEMS s d c z)
    asc_register_test(NAME "asc_cpp.dense_lapack.${suite}.${_scalar}"
      TARGET "${_target}" ARGUMENTS "${_scalar}"
      LABELS dense_lapack lapack runtime numerical validation allocation
      TIMEOUT 300)
  endforeach()
endfunction()
function(_asc_dense_lapack_add_band_expert_fault suite support)
  set(_target "asc_dense_lapack_${suite}_test")
  target_sources("${_target}" PRIVATE "${support}.cc")
  foreach(_routine IN LISTS ARGN)
    foreach(_scalar IN ITEMS s d c z)
      target_link_options("${_target}" PRIVATE
        "-Wl,--wrap=${_scalar}${_routine}_")
    endforeach()
  endforeach()
endfunction()
foreach(_suite IN ITEMS band_driver_equilibration
    band_driver_equilibration_fidelity band_driver_equilibration_validation
    band_condition band_condition_validation band_condition_fidelity
    band_condition_math_gate band_refinement band_refinement_edges
    band_refinement_fidelity band_refinement_math_gate band_refinement_validation
    band_expert band_expert_scaling band_expert_math_gate band_expert_fidelity
    band_expert_edges band_expert_validation band_expert_fault band_expert_alias
    band_unwritten_info)
  _asc_dense_lapack_add_band_expert_test("${_suite}")
endforeach()
_asc_dense_lapack_add_band_expert_fault(band_driver_equilibration_validation
  band_expert_fault_support pbsv pbequ)
_asc_dense_lapack_add_band_expert_fault(band_condition_validation
  band_condition_fault_support pbcon)
_asc_dense_lapack_add_band_expert_fault(band_refinement_validation
  band_refinement_fault_support pbrfs)
foreach(_suite IN ITEMS band_expert_validation band_expert_fault band_expert_alias)
  _asc_dense_lapack_add_band_expert_fault("${_suite}"
    band_expert_driver_fault_support pbsvx)
endforeach()
_asc_dense_lapack_add_band_expert_fault(band_unwritten_info
  band_unwritten_info_support pbsv pbequ pbcon pbrfs pbsvx)
foreach(_kind IN ITEMS expert estimation refinement driver)
  set(_target "asc_dense_lapack_band_${_kind}_limits_test")
  add_executable("${_target}" "band_${_kind}_limits_test.cc")
  _asc_dense_lapack_configure_test("${_target}")
  asc_register_test(NAME "asc_cpp.dense_lapack.band_${_kind}_integer_limits"
    TARGET "${_target}" LABELS dense_lapack lapack validation TIMEOUT 300)
endforeach()

# General-band expert paths: preserve all ordinary owner runtime selections.
function(_asc_dense_lapack_add_gb_expert_test suite)
  set(_target "asc_dense_lapack_${suite}_test")
  add_executable("${_target}" "${suite}_test.cc" allocation_audit.cc
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test("${_target}")
  target_include_directories("${_target}" PRIVATE
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  target_link_options("${_target}" PRIVATE
    "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
endfunction()
foreach(_suite IN ITEMS lu_band_equilibration lu_band_condition
    lu_band_refinement lu_band_solve lu_band_driver lu_band_driver_outcome
    lu_band_expert_fault lu_band_diagnostics)
  _asc_dense_lapack_add_gb_expert_test("${_suite}")
endforeach()
target_sources(asc_dense_lapack_lu_band_expert_fault_test PRIVATE
  lu_band_expert_faults.cc)
foreach(_scalar IN ITEMS s d c z)
  foreach(_routine IN ITEMS gbequ gbcon gbrfs gbsv gbsvx)
    target_link_options(asc_dense_lapack_lu_band_expert_fault_test PRIVATE
      "-Wl,--wrap=${_scalar}${_routine}_")
  endforeach()
  foreach(_suite IN ITEMS equilibration condition refinement driver_outcome)
    foreach(_mode IN ITEMS regular extreme)
      asc_register_test(
        NAME "asc_cpp.dense_lapack.lu_band_${_suite}.${_scalar}.${_mode}"
        TARGET "asc_dense_lapack_lu_band_${_suite}_test"
        ARGUMENTS "${_scalar}" "${_mode}"
        LABELS dense_lapack lapack runtime numerical allocation TIMEOUT 120)
    endforeach()
  endforeach()
  foreach(_suite IN ITEMS solve driver expert_fault)
    asc_register_test(NAME "asc_cpp.dense_lapack.lu_band_${_suite}.${_scalar}"
      TARGET "asc_dense_lapack_lu_band_${_suite}_test" ARGUMENTS "${_scalar}"
      LABELS dense_lapack lapack runtime numerical validation allocation
      TIMEOUT 120)
  endforeach()
  foreach(_route RANGE 0 6)
    asc_register_test(
      NAME "asc_cpp.dense_lapack.lu_band_expert_cold.${_scalar}.${_route}"
      TARGET asc_dense_lapack_lu_band_expert_fault_test
      ARGUMENTS "${_scalar}" "${_route}"
      LABELS dense_lapack lapack runtime validation allocation TIMEOUT 120)
  endforeach()
endforeach()
add_executable(asc_dense_lapack_lu_band_expert_counts_test
  lu_band_expert_counts_test.cc)
_asc_dense_lapack_configure_test(asc_dense_lapack_lu_band_expert_counts_test)
target_include_directories(asc_dense_lapack_lu_band_expert_counts_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME asc_cpp.dense_lapack.lu_band_expert_counts
  TARGET asc_dense_lapack_lu_band_expert_counts_test
  LABELS dense_lapack lapack validation TIMEOUT 120)
asc_register_test(NAME asc_cpp.dense_lapack.lu_band_diagnostics
  TARGET asc_dense_lapack_lu_band_diagnostics_test ARGUMENTS regular
  LABELS dense_lapack lapack runtime numerical allocation TIMEOUT 120)
# The external actual compiler-emission comparison remains a separate required
# ABI gate. A portable CXX pinned-signature probe does not replace that evidence.

# Portable pinned-signature check; external actual-emission replay stays required.
add_executable(asc_dense_lapack_lu_band_expert_pinned_signatures_test
  lu_band_expert_pinned_signatures_test.cc)
_asc_dense_lapack_configure_test(
  asc_dense_lapack_lu_band_expert_pinned_signatures_test)
target_include_directories(
  asc_dense_lapack_lu_band_expert_pinned_signatures_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME asc_cpp.dense_lapack.gb_expert_pinned_signatures
  TARGET asc_dense_lapack_lu_band_expert_pinned_signatures_test
  LABELS dense_lapack lapack validation TIMEOUT 30)

# Actual GBEQUB is separate from GBEQU. Preserve every owner runtime mode;
# actual compiler-emitted prototype evidence remains external and required.
foreach(_suite IN ITEMS lu_band_equilibration_radix
    lu_band_equilibration_radix_fault lu_band_equilibration_radix_native)
  _asc_dense_lapack_add_gb_expert_test("${_suite}")
endforeach()
target_sources(asc_dense_lapack_lu_band_equilibration_radix_fault_test PRIVATE
  lu_band_equilibration_radix_faults.cc)
foreach(_scalar IN ITEMS s d c z)
  target_link_options(asc_dense_lapack_lu_band_equilibration_radix_fault_test
    PRIVATE "-Wl,--wrap=${_scalar}gbequb_")
  foreach(_mode IN ITEMS regular scaled amax extreme)
    asc_register_test(NAME "asc_cpp.dense_lapack.gb_equb.${_scalar}.${_mode}"
      TARGET asc_dense_lapack_lu_band_equilibration_radix_test
      ARGUMENTS "${_scalar}" "${_mode}"
      LABELS dense_lapack lapack runtime numerical allocation TIMEOUT 120)
  endforeach()
  foreach(_mode IN ITEMS cold fault)
    asc_register_test(NAME "asc_cpp.dense_lapack.gb_equb.${_scalar}.${_mode}"
      TARGET asc_dense_lapack_lu_band_equilibration_radix_fault_test
      ARGUMENTS "${_scalar}" "${_mode}"
      LABELS dense_lapack lapack runtime validation allocation TIMEOUT 120)
  endforeach()
  foreach(_mode IN ITEMS math nonfinite)
    asc_register_test(NAME "asc_cpp.dense_lapack.gb_equb.${_scalar}.native_${_mode}"
      TARGET asc_dense_lapack_lu_band_equilibration_radix_native_test
      ARGUMENTS "${_scalar}" "${_mode}"
      LABELS dense_lapack lapack runtime numerical validation TIMEOUT 120)
  endforeach()
endforeach()
add_executable(asc_dense_lapack_lu_band_equilibration_radix_pinned_signatures_test
  lu_band_equilibration_radix_pinned_signatures_test.cc)
_asc_dense_lapack_configure_test(
  asc_dense_lapack_lu_band_equilibration_radix_pinned_signatures_test)
target_include_directories(
  asc_dense_lapack_lu_band_equilibration_radix_pinned_signatures_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
asc_register_test(NAME asc_cpp.dense_lapack.gb_equb_pinned_signatures
  TARGET asc_dense_lapack_lu_band_equilibration_radix_pinned_signatures_test
  LABELS dense_lapack lapack validation TIMEOUT 30)

if(ASC_CPP_INSTALL)
  string(REPLACE ";" "|" _lapack_runtimes "${ASC_CPP_LAPACK_RUNTIME_LIBRARIES}")
  add_test(NAME asc_cpp.dense_lapack.package_integration
    COMMAND "${CMAKE_COMMAND}"
      "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
      "-DPRODUCER_BINARY_DIR:PATH=${PROJECT_BINARY_DIR}"
      "-DWORK_DIR:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}/reference provider package"
      "-DASC_CPP_TEST_WORKSPACE_ROOT:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}"
      "-DASC_CPP_TEST_WORKSPACE_GUARD:FILEPATH=${ASC_CPP_TEST_WORKSPACE_GUARD}"
      "-DLAPACK_ROOT:PATH=${ASC_CPP_LAPACK_ROOT}"
      "-DLAPACK_RUNTIMES:STRING=${_lapack_runtimes}"
      "-DCXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
      "-DCONFIG:STRING=$<CONFIG>"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/package_integration_test.cmake")
  set_tests_properties(asc_cpp.dense_lapack.package_integration PROPERTIES
    LABELS "dense_lapack;lapack;package;consumer;isolation"
    RUN_SERIAL TRUE TIMEOUT 300)
endif()

string(REPLACE ";" "|" _lapack_runtimes "${ASC_CPP_LAPACK_RUNTIME_LIBRARIES}")
add_test(NAME asc_cpp.dense_lapack.configuration_gates
  COMMAND "${CMAKE_COMMAND}"
    "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    "-DWORK_DIR:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}/reference configuration gates"
    "-DASC_CPP_TEST_WORKSPACE_ROOT:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}"
    "-DASC_CPP_TEST_WORKSPACE_GUARD:FILEPATH=${ASC_CPP_TEST_WORKSPACE_GUARD}"
    "-DASCCMAKE_DIR:PATH=${ASCCMake_DIR}"
    "-DCXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
    "-DFORTRAN_COMPILER:FILEPATH=${CMAKE_Fortran_COMPILER}"
    "-DFORTRAN_FLAGS:STRING=${CMAKE_Fortran_FLAGS}"
    "-DINTEGER_BITS:STRING=${ASC_CPP_LAPACK_INTEGER_BITS}"
    "-DLAPACK_ROOT:PATH=${ASC_CPP_LAPACK_ROOT}"
    "-DLAPACK_ATTESTATION:FILEPATH=${ASC_CPP_LAPACK_ATTESTATION}"
    "-DLAPACK_RUNTIMES:STRING=${_lapack_runtimes}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/configuration_gate_test.cmake")
set_tests_properties(asc_cpp.dense_lapack.configuration_gates PROPERTIES
  LABELS "dense_lapack;lapack;package;configuration;isolation"
  RUN_SERIAL TRUE TIMEOUT 240)

foreach(_header IN ITEMS lapack.h lapack_export.h lapack_lu.h
    lapack_lu_equilibration.h lapack_lu_condition.h lapack_lu_refinement.h
    lapack_lu_driver.h lapack_lu_helpers.h lapack_cholesky.h lapack_qr.h
    lapack_cholesky_condition.h
    lapack_cholesky_driver.h
    lapack_cholesky_equilibration.h
    lapack_cholesky_refinement.h
    lapack_least_squares.h lapack_cholesky_band.h lapack_indefinite.h
    lapack_rank_revealing.h lapack_sylvester.h
    lapack_indefinite_condition.h lapack_indefinite_driver.h
    lapack_indefinite_refinement.h lapack_svd_least_squares.h
    lapack_lu_band.h
    lapack_general_band.h
    lapack_lu_band_condition.h
    lapack_lu_band_driver.h
    lapack_lu_band_equilibration.h
    lapack_lu_band_equilibration_radix.h
    lapack_lu_band_expert.h
    lapack_lu_band_refinement.h
    lapack_tridiagonal.h lapack_tridiagonal_condition.h lapack_tridiagonal_driver.h lapack_tridiagonal_refinement.h
    lapack_cholesky_band_condition.h
    lapack_cholesky_band_driver.h
    lapack_cholesky_band_equilibration.h
    lapack_cholesky_band_expert.h
    lapack_cholesky_band_refinement.h)
  string(MAKE_C_IDENTIFIER "${_header}" _id)
  foreach(_mode IN ITEMS normal no_exceptions)
    set(_target "asc_dense_lapack_header_${_id}_${_mode}")
    add_executable("${_target}"
      "${PROJECT_SOURCE_DIR}/tests/compile/header_self_containment.cc")
    _asc_dense_lapack_configure_test("${_target}")
    target_compile_definitions("${_target}" PRIVATE
      "ASC_TEST_HEADER=\"asc/dense/providers/${_header}\"")
    if(_mode STREQUAL "no_exceptions")
      target_compile_options("${_target}" PRIVATE -fno-exceptions)
    endif()
    asc_register_test(NAME "asc_cpp.dense_lapack.header.${_id}.${_mode}"
      TARGET "${_target}" LABELS dense_lapack lapack compile)
  endforeach()
endforeach()

# Fault tests run the real routine before withholding its INFO output.
set(_asc_lu_aux_info_wrap)
foreach(_scalar IN ITEMS s d c z)
  foreach(_routine IN ITEMS gecon geequ geequb gerfs gesvx)
    list(APPEND _asc_lu_aux_info_wrap "-Wl,--wrap=${_scalar}${_routine}_")
  endforeach()
endforeach()
foreach(_family IN ITEMS condition equilibration refinement driver)
  set(_target "asc_dense_lapack_lu_${_family}_info_test")
  add_executable("${_target}" "lu_${_family}_info_test.cc"
    lu_aux_info_faults.cc lu_aux_info_test_support.cc allocation_audit.cc
    "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
  _asc_dense_lapack_configure_test("${_target}")
  target_include_directories("${_target}" PRIVATE
    "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}"
    "${ASC_CPP_LAPACK_ROOT}/include")
  target_link_options("${_target}" PRIVATE ${_asc_lu_aux_info_wrap}
    "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
  foreach(_scalar IN ITEMS s d c z)
    asc_register_test(NAME "asc_cpp.dense_lapack.lu_${_family}_info.${_scalar}"
      TARGET "${_target}" ARGUMENTS "${_scalar}"
      LABELS dense_lapack lapack runtime numerical allocation TIMEOUT 120)
  endforeach()
endforeach()
unset(_asc_lu_aux_info_wrap)

# GESVX output-pivot faults preserve real numerical computation and INFO.
add_executable(asc_dense_lapack_lu_driver_pivot_test
  lu_driver_pivot_test.cc lu_driver_pivot_faults.cc allocation_audit.cc
  "${PROJECT_SOURCE_DIR}/tests/dense/allocation_probe.cc")
_asc_dense_lapack_configure_test(asc_dense_lapack_lu_driver_pivot_test)
target_include_directories(asc_dense_lapack_lu_driver_pivot_test PRIVATE
  "${PROJECT_SOURCE_DIR}" "${ASC_CPP_LAPACK_CONFIG_DIR}"
  "${ASC_CPP_LAPACK_ROOT}/include")
target_link_options(asc_dense_lapack_lu_driver_pivot_test PRIVATE
  "-Wl,--wrap=sgesvx_,--wrap=dgesvx_,--wrap=cgesvx_,--wrap=zgesvx_"
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign,--wrap=free")
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.lu_driver_pivot.${_scalar}"
    TARGET asc_dense_lapack_lu_driver_pivot_test ARGUMENTS "${_scalar}"
    LABELS dense_lapack lapack runtime numerical allocation TIMEOUT 120)
endforeach()
