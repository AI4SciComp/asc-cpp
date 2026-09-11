find_package(Threads REQUIRED)
add_executable(asc_lu_condition_modes_test lu_condition_modes_test.cc)
_asc_dense_lapack_configure_test(asc_lu_condition_modes_test)
target_link_libraries(asc_lu_condition_modes_test PRIVATE Threads::Threads)
target_include_directories(asc_lu_condition_modes_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.lu_condition_modes.concurrency_${_scalar}"
    TARGET asc_lu_condition_modes_test ARGUMENTS "${_scalar}" concurrency
    LABELS dense_lapack lapack lu_condition numerical concurrency TIMEOUT 120)
  asc_register_test(NAME "asc_cpp.dense_lapack.lu_condition_modes.required_math_${_scalar}"
    TARGET asc_lu_condition_modes_test ARGUMENTS "${_scalar}" extreme
    LABELS dense_lapack lapack lu_condition numerical TIMEOUT 120)
endforeach()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(asc_lu_condition_observation_test lu_condition_observation_test.cc)
  _asc_dense_lapack_configure_test(asc_lu_condition_observation_test)
  target_include_directories(asc_lu_condition_observation_test PRIVATE
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  target_link_options(asc_lu_condition_observation_test PRIVATE
    "-Wl,--wrap=sgecon_,--wrap=dgecon_,--wrap=cgecon_,--wrap=zgecon_")
  asc_register_test(NAME asc_cpp.dense_lapack.lu_condition_modes.observation
    TARGET asc_lu_condition_observation_test
    LABELS dense_lapack lapack lu_condition observation validation TIMEOUT 240)
endif()
