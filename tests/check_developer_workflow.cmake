if(NOT DEFINED HOUIO_SOURCE_DIR)
    message(FATAL_ERROR "HOUIO_SOURCE_DIR is required")
endif()

set(dev_script "${HOUIO_SOURCE_DIR}/tools/dev.ps1")
if(NOT EXISTS "${dev_script}")
    message(FATAL_ERROR "Developer workflow script is missing: ${dev_script}")
endif()

execute_process(
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File "${dev_script}" help
    RESULT_VARIABLE help_result
    OUTPUT_VARIABLE help_output
    ERROR_VARIABLE help_error
)
if(NOT help_result EQUAL 0)
    message(FATAL_ERROR "Developer workflow help failed:\n${help_output}\n${help_error}")
endif()
if(NOT help_output MATCHES "windows-msvc-core-minimal")
    message(FATAL_ERROR "Developer workflow help does not list the product profiles")
endif()
if(NOT help_output MATCHES "Clean scopes")
    message(FATAL_ERROR "Developer workflow help does not document cleanup scopes")
endif()

file(READ "${HOUIO_SOURCE_DIR}/CMakePresets.json" preset_document)
foreach(required_profile IN ITEMS
    windows-msvc-core-minimal
    linux-gcc-core-openvdb
    windows-msvc-scene-io
    windows-msvc-full-development)
    if(NOT preset_document MATCHES "\\\"${required_profile}\\\"")
        message(FATAL_ERROR "Missing product build profile: ${required_profile}")
    endif()
endforeach()

execute_process(
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File "${dev_script}"
        clean -Preset houio-nonexistent-clean-validation
    RESULT_VARIABLE clean_result
    OUTPUT_VARIABLE clean_output
    ERROR_VARIABLE clean_error
)
if(NOT clean_result EQUAL 0)
    message(FATAL_ERROR "Non-destructive preset cleanup failed:\n${clean_output}\n${clean_error}")
endif()
if(NOT clean_output MATCHES "already clean")
    message(FATAL_ERROR "Non-destructive preset cleanup did not report its result")
endif()

execute_process(
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File "${dev_script}"
        clean -Preset ..
    RESULT_VARIABLE traversal_result
    OUTPUT_VARIABLE traversal_output
    ERROR_VARIABLE traversal_error
)
if(traversal_result EQUAL 0)
    message(FATAL_ERROR "Preset cleanup accepted a path traversal value")
endif()
set(traversal_message "${traversal_output}\n${traversal_error}")
if(NOT traversal_message MATCHES "requires a preset name, not a path")
    message(FATAL_ERROR "Preset cleanup path guard did not explain the rejection")
endif()

execute_process(
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File "${dev_script}"
        clean -CleanScope dependencies
    RESULT_VARIABLE guarded_result
    OUTPUT_VARIABLE guarded_output
    ERROR_VARIABLE guarded_error
)
if(guarded_result EQUAL 0)
    message(FATAL_ERROR "Dependency cleanup succeeded without -ConfirmClean")
endif()
set(guarded_message "${guarded_output}\n${guarded_error}")
if(NOT guarded_message MATCHES "requires -ConfirmClean")
    message(FATAL_ERROR "Dependency cleanup guard did not explain the required confirmation")
endif()
