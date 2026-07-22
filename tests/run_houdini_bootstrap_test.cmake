if(NOT WIN32)
    message(FATAL_ERROR "HouIO bootstrap validation is Windows-only")
endif()
if(NOT DEFINED HOUIO_BINARY_DIR)
    message(FATAL_ERROR "HOUIO_BINARY_DIR is required")
endif()
if(NOT DEFINED HOUIO_BUILD_CONFIG)
    set(HOUIO_BUILD_CONFIG "Release")
endif()
if(NOT DEFINED HOUIO_HOUDINI_PACKAGE_ARCHIVE)
    message(FATAL_ERROR "HOUIO_HOUDINI_PACKAGE_ARCHIVE is required")
endif()
if(NOT DEFINED HOUIO_HOUDINI_BOOTSTRAP_EXTRACT_DIR)
    message(FATAL_ERROR "HOUIO_HOUDINI_BOOTSTRAP_EXTRACT_DIR is required")
endif()
set(houio_process_timeout 600)
execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        --build "${HOUIO_BINARY_DIR}"
        --config "${HOUIO_BUILD_CONFIG}"
        --target houio_houdini_package
    TIMEOUT ${houio_process_timeout}
    RESULT_VARIABLE package_build_result
    OUTPUT_VARIABLE package_build_output
    ERROR_VARIABLE package_build_error
)
if(NOT package_build_result EQUAL 0)
    message(FATAL_ERROR "Package build failed:\n${package_build_output}\n${package_build_error}")
endif()
if(NOT EXISTS "${HOUIO_HOUDINI_PACKAGE_ARCHIVE}")
    message(FATAL_ERROR "Package archive does not exist after the build: ${HOUIO_HOUDINI_PACKAGE_ARCHIVE}")
endif()

find_program(HOUIO_POWERSHELL_EXECUTABLE NAMES powershell pwsh REQUIRED)

file(REMOVE_RECURSE "${HOUIO_HOUDINI_BOOTSTRAP_EXTRACT_DIR}")
file(MAKE_DIRECTORY "${HOUIO_HOUDINI_BOOTSTRAP_EXTRACT_DIR}")
file(
    ARCHIVE_EXTRACT
    INPUT "${HOUIO_HOUDINI_PACKAGE_ARCHIVE}"
    DESTINATION "${HOUIO_HOUDINI_BOOTSTRAP_EXTRACT_DIR}"
)

set(bootstrap_script "${HOUIO_HOUDINI_BOOTSTRAP_EXTRACT_DIR}/bootstrap_houdini_package.ps1")
set(bootstrap_directory "${HOUIO_HOUDINI_BOOTSTRAP_EXTRACT_DIR}/bootstrap")
if(NOT EXISTS "${bootstrap_script}")
    message(FATAL_ERROR "Bootstrap script is missing from the package archive")
endif()

set(existing_bootstrap_directory "${HOUIO_HOUDINI_BOOTSTRAP_EXTRACT_DIR}/existing-bootstrap")
set(existing_bootstrap_sentinel "${existing_bootstrap_directory}/sentinel.txt")
file(MAKE_DIRECTORY "${existing_bootstrap_directory}")
file(WRITE "${existing_bootstrap_sentinel}" "caller-owned")
execute_process(
    COMMAND
        "${HOUIO_POWERSHELL_EXECUTABLE}"
        -NoProfile
        -ExecutionPolicy Bypass
        -File "${bootstrap_script}"
        -ValidateOnly
        -BootstrapDirectory "${existing_bootstrap_directory}"
    TIMEOUT ${houio_process_timeout}
    RESULT_VARIABLE existing_bootstrap_result
    OUTPUT_VARIABLE existing_bootstrap_output
    ERROR_VARIABLE existing_bootstrap_error
)
if(existing_bootstrap_result EQUAL 0)
    message(FATAL_ERROR "Bootstrap accepted a caller-owned existing directory")
endif()
if(NOT EXISTS "${existing_bootstrap_sentinel}")
    message(FATAL_ERROR "Bootstrap deleted caller-owned existing directory contents")
endif()
file(REMOVE_RECURSE "${existing_bootstrap_directory}")

execute_process(
    COMMAND
        "${HOUIO_POWERSHELL_EXECUTABLE}"
        -NoProfile
        -ExecutionPolicy Bypass
        -File "${bootstrap_script}"
        -ValidateOnly
        -KeepBootstrap
        -BootstrapDirectory "${bootstrap_directory}"
    TIMEOUT ${houio_process_timeout}
    RESULT_VARIABLE bootstrap_result
    OUTPUT_VARIABLE bootstrap_output
    ERROR_VARIABLE bootstrap_error
)

if(NOT bootstrap_result EQUAL 0)
    message(FATAL_ERROR "Bootstrap validation failed:\n${bootstrap_output}\n${bootstrap_error}")
endif()
if(NOT bootstrap_output MATCHES "No package files were installed")
    message(FATAL_ERROR "Bootstrap output did not confirm the non-installing workflow:\n${bootstrap_output}")
endif()

set(loader_path "${bootstrap_directory}/packages/houio_loader.json")
if(NOT EXISTS "${loader_path}")
    message(FATAL_ERROR "Bootstrap loader was not created: ${loader_path}")
endif()

file(READ "${loader_path}" loader_json)
string(JSON loader_enabled GET "${loader_json}" enable)
string(JSON loader_package_path GET "${loader_json}" package_path)
if(NOT loader_enabled)
    message(FATAL_ERROR "Bootstrap loader is disabled")
endif()
file(TO_CMAKE_PATH "${HOUIO_HOUDINI_BOOTSTRAP_EXTRACT_DIR}" expected_package_path)
if(NOT loader_package_path STREQUAL expected_package_path)
    message(FATAL_ERROR "Bootstrap loader points to '${loader_package_path}', expected '${expected_package_path}'")
endif()

execute_process(
    COMMAND
        "${HOUIO_POWERSHELL_EXECUTABLE}"
        -NoProfile
        -ExecutionPolicy Bypass
        -File "${HOUIO_HOUDINI_BOOTSTRAP_EXTRACT_DIR}/install_houdini_package.ps1"
    TIMEOUT ${houio_process_timeout}
    RESULT_VARIABLE installer_result
    OUTPUT_VARIABLE installer_output
    ERROR_VARIABLE installer_error
)
if(installer_result EQUAL 0)
    message(FATAL_ERROR "Installer succeeded without an explicit -Install or -Uninstall action")
endif()
if(NOT installer_error MATCHES "No action selected")
    message(FATAL_ERROR "Installer did not fail with the expected safe-action message:\n${installer_output}\n${installer_error}")
endif()

set(legacy_home "${HOUIO_HOUDINI_BOOTSTRAP_EXTRACT_DIR}/legacy-home")
set(legacy_loader "${legacy_home}/houdini99.9/packages/houio_loader.json")
file(MAKE_DIRECTORY "${legacy_home}/houdini99.9/packages")
file(WRITE "${legacy_loader}" "{}")
execute_process(
    COMMAND
        "${CMAKE_COMMAND}" -E env
        "HOME=${legacy_home}"
        "USERPROFILE=${legacy_home}"
        "${HOUIO_POWERSHELL_EXECUTABLE}"
        -NoProfile
        -ExecutionPolicy Bypass
        -File "${HOUIO_HOUDINI_BOOTSTRAP_EXTRACT_DIR}/install_houdini_package.ps1"
        -Uninstall
        -KeepFiles
        -HoudiniVersions 99.9
        -InstallDirectory "${HOUIO_HOUDINI_BOOTSTRAP_EXTRACT_DIR}/unused-install"
    TIMEOUT ${houio_process_timeout}
    RESULT_VARIABLE legacy_cleanup_result
    OUTPUT_VARIABLE legacy_cleanup_output
    ERROR_VARIABLE legacy_cleanup_error
)
if(NOT legacy_cleanup_result EQUAL 0)
    message(FATAL_ERROR "Legacy loader cleanup failed:\n${legacy_cleanup_output}\n${legacy_cleanup_error}")
endif()
if(EXISTS "${legacy_loader}")
    message(FATAL_ERROR "Installer did not remove the legacy HOME loader")
endif()

file(REMOVE_RECURSE "${bootstrap_directory}")
message(STATUS "HouIO transient bootstrap validation passed")
