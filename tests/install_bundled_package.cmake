if(NOT DEFINED HOUIO_BINARY_DIR OR HOUIO_BINARY_DIR STREQUAL "")
    message(FATAL_ERROR "HOUIO_BINARY_DIR is required")
endif()
if(NOT DEFINED HOUIO_INSTALL_ROOT OR HOUIO_INSTALL_ROOT STREQUAL "")
    message(FATAL_ERROR "HOUIO_INSTALL_ROOT is required")
endif()

file(REMOVE_RECURSE "${HOUIO_INSTALL_ROOT}")
set(install_command
    "${CMAKE_COMMAND}"
    --install "${HOUIO_BINARY_DIR}"
    --prefix "${HOUIO_INSTALL_ROOT}"
)
if(DEFINED HOUIO_BUILD_CONFIG AND NOT HOUIO_BUILD_CONFIG STREQUAL "")
    list(APPEND install_command --config "${HOUIO_BUILD_CONFIG}")
endif()

execute_process(
    COMMAND ${install_command}
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR
        "Bundled HouIO install failed with ${install_result}:\n"
        "${install_output}\n${install_error}")
endif()
