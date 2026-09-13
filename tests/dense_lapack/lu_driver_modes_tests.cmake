find_package(Threads REQUIRED)
add_executable(asc_lu_driver_modes_test lu_driver_modes_test.cc)
_asc_dense_lapack_configure_test(asc_lu_driver_modes_test)
target_link_libraries(asc_lu_driver_modes_test PRIVATE Threads::Threads)
foreach(_scalar IN ITEMS s d c z)
  asc_register_test(NAME "asc_cpp.dense_lapack.lu_driver_modes.concurrency_${_scalar}"
    TARGET asc_lu_driver_modes_test ARGUMENTS "${_scalar}" concurrency
    LABELS dense_lapack lapack lu_driver numerical concurrency TIMEOUT 120)
  asc_register_test(NAME "asc_cpp.dense_lapack.lu_driver_modes.required_math_${_scalar}"
    TARGET asc_lu_driver_modes_test ARGUMENTS "${_scalar}" extreme
    LABELS dense_lapack lapack lu_driver numerical TIMEOUT 120)
endforeach()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(asc_lu_driver_observation_test lu_driver_observation_test.cc)
  _asc_dense_lapack_configure_test(asc_lu_driver_observation_test)
  target_include_directories(asc_lu_driver_observation_test PRIVATE
    "${ASC_CPP_LAPACK_CONFIG_DIR}" "${ASC_CPP_LAPACK_ROOT}/include")
  target_link_options(asc_lu_driver_observation_test PRIVATE
    "-Wl,--wrap=sgesvx_,--wrap=dgesvx_,--wrap=cgesvx_,--wrap=zgesvx_")
  asc_register_test(NAME asc_cpp.dense_lapack.lu_driver_modes.observation
    TARGET asc_lu_driver_observation_test
    LABELS dense_lapack lapack lu_driver observation validation TIMEOUT 240)
endif()
