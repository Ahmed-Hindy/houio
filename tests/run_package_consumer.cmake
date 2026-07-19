foreach(required_variable IN ITEMS HOUIO_BINARY_DIR HOUIO_SOURCE_DIR HOUIO_GENERATOR)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "Missing required variable: ${required_variable}")
    endif()
endforeach()

set(package_root "${HOUIO_BINARY_DIR}/package-consumer/install")
set(consumer_build_dir "${HOUIO_BINARY_DIR}/package-consumer/build")
set(consumer_source_dir "${HOUIO_SOURCE_DIR}/tests/package_consumer")
file(REMOVE_RECURSE "${HOUIO_BINARY_DIR}/package-consumer")

set(install_command "${CMAKE_COMMAND}" --install "${HOUIO_BINARY_DIR}" --prefix "${package_root}")
if(DEFINED HOUIO_BUILD_CONFIG AND NOT "${HOUIO_BUILD_CONFIG}" STREQUAL "")
    list(APPEND install_command --config "${HOUIO_BUILD_CONFIG}")
endif()

execute_process(
    COMMAND ${install_command}
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "HouIO installation failed:\n${install_output}\n${install_error}")
endif()

set(
    configure_command
    "${CMAKE_COMMAND}"
    -S "${consumer_source_dir}"
    -B "${consumer_build_dir}"
    -G "${HOUIO_GENERATOR}"
    "-DCMAKE_PREFIX_PATH=${package_root}"
)
if(DEFINED HOUIO_BUILD_TYPE AND NOT "${HOUIO_BUILD_TYPE}" STREQUAL "")
    list(APPEND configure_command "-DCMAKE_BUILD_TYPE=${HOUIO_BUILD_TYPE}")
endif()
if(DEFINED HOUIO_CXX_COMPILER AND NOT "${HOUIO_CXX_COMPILER}" STREQUAL "")
    list(APPEND configure_command "-DCMAKE_CXX_COMPILER=${HOUIO_CXX_COMPILER}")
endif()

execute_process(
    COMMAND ${configure_command}
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error
)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR "Package consumer configuration failed:\n${configure_output}\n${configure_error}")
endif()

set(build_command "${CMAKE_COMMAND}" --build "${consumer_build_dir}" --parallel)
if(DEFINED HOUIO_BUILD_CONFIG AND NOT "${HOUIO_BUILD_CONFIG}" STREQUAL "")
    list(APPEND build_command --config "${HOUIO_BUILD_CONFIG}")
endif()

execute_process(
    COMMAND ${build_command}
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error
)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "Package consumer build failed:\n${build_output}\n${build_error}")
endif()

set(executable_suffix "")
if(CMAKE_HOST_WIN32)
    set(executable_suffix ".exe")
endif()

set(executable_candidates "${consumer_build_dir}/houio_package_consumer${executable_suffix}")
if(DEFINED HOUIO_BUILD_CONFIG AND NOT "${HOUIO_BUILD_CONFIG}" STREQUAL "")
    list(APPEND executable_candidates
        "${consumer_build_dir}/${HOUIO_BUILD_CONFIG}/houio_package_consumer${executable_suffix}"
    )
endif()

set(consumer_executable "")
foreach(candidate IN LISTS executable_candidates)
    if(EXISTS "${candidate}")
        set(consumer_executable "${candidate}")
        break()
    endif()
endforeach()
if(consumer_executable STREQUAL "")
    message(FATAL_ERROR "Package consumer executable was not produced")
endif()

execute_process(
    COMMAND "${consumer_executable}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_output
    ERROR_VARIABLE run_error
)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "Package consumer execution failed:\n${run_output}\n${run_error}")
endif()
