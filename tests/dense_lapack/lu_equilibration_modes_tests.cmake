find_package(Threads REQUIRED)
add_executable(asc_lu_equilibration_modes_test lu_equilibration_modes_test.cc)
_asc_dense_lapack_configure_test(asc_lu_equilibration_modes_test)
target_link_libraries(asc_lu_equilibration_modes_test PRIVATE Threads::Threads)
target_include_directories(asc_lu_equilibration_modes_test PRIVATE
  "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.lu_equilibration_modes.concurrency_${_scalar}"
    TARGET asc_lu_equilibration_modes_test ARGUMENTS "${_scalar}" concurrency
    LABELS dense_lapack lapack lu_equilibration numerical concurrency TIMEOUT 120)
  asc_register_test(NAME "asc_cpp.dense_lapack.lu_equilibration_modes.required_math_${_scalar}"
    TARGET asc_lu_equilibration_modes_test ARGUMENTS "${_scalar}" extreme
    LABELS dense_lapack lapack lu_equilibration numerical TIMEOUT 120)
endforeach()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(asc_lu_equilibration_observation_test lu_equilibration_observation_test.cc)
  _asc_dense_lapack_configure_test(asc_lu_equilibration_observation_test)
  target_include_directories(asc_lu_equilibration_observation_test PRIVATE
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  target_link_options(asc_lu_equilibration_observation_test PRIVATE
    "-Wl,--wrap=sgeequ_,--wrap=dgeequ_,--wrap=cgeequ_,--wrap=zgeequ_"
    "-Wl,--wrap=sgeequb_,--wrap=dgeequb_,--wrap=cgeequb_,--wrap=zgeequb_")
  asc_register_test(NAME asc_cpp.dense_lapack.lu_equilibration_modes.observation
    TARGET asc_lu_equilibration_observation_test
    LABELS dense_lapack lapack lu_equilibration observation validation TIMEOUT 240)
endif()
