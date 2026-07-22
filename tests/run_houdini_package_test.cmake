if(NOT DEFINED HOUIO_HOUDINI_PACKAGE_ARCHIVE)
    message(FATAL_ERROR "HOUIO_HOUDINI_PACKAGE_ARCHIVE is required")
endif()
if(NOT DEFINED HOUIO_HOUDINI_PACKAGE_EXTRACT_DIR)
    message(FATAL_ERROR "HOUIO_HOUDINI_PACKAGE_EXTRACT_DIR is required")
endif()
if(NOT DEFINED HOUIO_HYTHON_EXECUTABLE)
    message(FATAL_ERROR "HOUIO_HYTHON_EXECUTABLE is required")
endif()
if(NOT DEFINED HOUIO_HOUDINI_PACKAGE_TEST_SCRIPT)
    message(FATAL_ERROR "HOUIO_HOUDINI_PACKAGE_TEST_SCRIPT is required")
endif()

if(NOT EXISTS "${HOUIO_HOUDINI_PACKAGE_ARCHIVE}")
    message(FATAL_ERROR "Package archive does not exist: ${HOUIO_HOUDINI_PACKAGE_ARCHIVE}")
endif()

file(REMOVE_RECURSE "${HOUIO_HOUDINI_PACKAGE_EXTRACT_DIR}")
file(MAKE_DIRECTORY "${HOUIO_HOUDINI_PACKAGE_EXTRACT_DIR}")
file(
    ARCHIVE_EXTRACT
    INPUT "${HOUIO_HOUDINI_PACKAGE_ARCHIVE}"
    DESTINATION "${HOUIO_HOUDINI_PACKAGE_EXTRACT_DIR}"
)

set(
    isolated_user_preferences
    "${HOUIO_HOUDINI_PACKAGE_EXTRACT_DIR}/user/houdini__HVER__"
)
execute_process(
    COMMAND
        "${CMAKE_COMMAND}" -E env
        "HOUDINI_PACKAGE_DIR=${HOUIO_HOUDINI_PACKAGE_EXTRACT_DIR}"
        "HOUDINI_USER_PREF_DIR=${isolated_user_preferences}"
        "HOUDINI_NO_ENV_FILE=1"
        "${HOUIO_HYTHON_EXECUTABLE}"
        "${HOUIO_HOUDINI_PACKAGE_TEST_SCRIPT}"
    RESULT_VARIABLE test_result
    OUTPUT_VARIABLE test_output
    ERROR_VARIABLE test_error
)

if(NOT test_output STREQUAL "")
    message(STATUS "${test_output}")
endif()
if(NOT test_error STREQUAL "")
    message(STATUS "${test_error}")
endif()
if(NOT test_result EQUAL 0)
    message(FATAL_ERROR "HouIO Houdini package test failed with exit code ${test_result}")
endif()
