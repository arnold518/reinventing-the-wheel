if(NOT DEFINED EXECUTABLE)
    message(FATAL_ERROR "EXECUTABLE is required")
endif()
execute_process(
    COMMAND "${EXECUTABLE}" --floors definitely.missing.contract
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 1 OR NOT error MATCHES "No selected parts were compared")
    message(FATAL_ERROR "Invalid selection did not fail explicitly: ${result}\n${output}\n${error}")
endif()
if(output MATCHES "DelayRuleTest passed")
    message(FATAL_ERROR "Zero comparisons were reported as passing")
endif()
