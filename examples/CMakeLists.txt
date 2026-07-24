add_executable(asc_cpp_quickstart quickstart.cc)
if(ASC_CPP_ENABLE_CUDA)
  set_source_files_properties(quickstart.cc PROPERTIES LANGUAGE CUDA)
endif()
target_link_libraries(asc_cpp_quickstart PRIVATE ASC::cpp)
target_link_libraries(
  asc_cpp_quickstart PRIVATE "$<BUILD_INTERFACE:asc_cpp_project_options>"
)
