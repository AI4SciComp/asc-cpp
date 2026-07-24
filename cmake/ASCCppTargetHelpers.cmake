include_guard(GLOBAL)

function(asc_cpp_configure_compiled_target target export_name)
  set_target_properties(
    "${target}"
    PROPERTIES
      EXPORT_NAME "${export_name}"
      VERSION "${PROJECT_VERSION}"
      SOVERSION "${PROJECT_VERSION_MAJOR}"
  )
  target_compile_features("${target}" PUBLIC cxx_std_20)
  target_include_directories(
    "${target}"
    PUBLIC
      "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
      "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/generated>"
      "$<INSTALL_INTERFACE:include>"
  )
  target_link_libraries(
    "${target}" PRIVATE "$<BUILD_INTERFACE:asc_cpp_project_options>"
  )

  if(BUILD_SHARED_LIBS)
    target_compile_definitions(
      "${target}" PUBLIC ASC_SHARED_BUILD PRIVATE ASC_BUILDING_LIBRARY
    )
    set_target_properties("${target}" PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    if(APPLE)
      set_target_properties("${target}" PROPERTIES INSTALL_RPATH "@loader_path")
    elseif(UNIX)
      set_target_properties("${target}" PROPERTIES INSTALL_RPATH "$ORIGIN")
    endif()
  endif()

  if(ASC_CPP_ENABLE_CUDA)
    get_target_property(_asc_cpp_sources "${target}" SOURCES)
    set_source_files_properties(${_asc_cpp_sources} PROPERTIES LANGUAGE CUDA)
    target_compile_options(
      "${target}"
      PUBLIC
        "$<$<COMPILE_LANGUAGE:CUDA>:--extended-lambda>"
        "$<$<COMPILE_LANGUAGE:CUDA>:--expt-relaxed-constexpr>"
    )
  endif()
endfunction()

function(asc_cpp_configure_interface_target target export_name)
  set_target_properties("${target}" PROPERTIES EXPORT_NAME "${export_name}")
  target_compile_features("${target}" INTERFACE cxx_std_20)
  target_include_directories(
    "${target}"
    INTERFACE
      "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
      "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/generated>"
      "$<INSTALL_INTERFACE:include>"
  )
endfunction()
