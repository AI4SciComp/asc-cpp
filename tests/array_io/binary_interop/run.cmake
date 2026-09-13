cmake_minimum_required(VERSION 3.25)
string(RANDOM LENGTH 24 ALPHABET 0123456789abcdef _suffix)
execute_process(COMMAND "${PYTHON}" -B "${TOOL_DIR}/check_binary_interop.py"
  --executable "${EXECUTABLE}" --component "${COMPONENT}"
  --work-dir "${WORK_ROOT}/${_suffix}" RESULT_VARIABLE _result)
if(NOT _result EQUAL 0)
  message(FATAL_ERROR "Independent binary ${COMPONENT} check failed: ${_result}")
endif()
