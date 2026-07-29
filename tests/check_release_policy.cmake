if(NOT DEFINED HOUIO_SOURCE_DIR OR HOUIO_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "HOUIO_SOURCE_DIR is required")
endif()

foreach(required_document IN ITEMS
    "LICENSE_STATUS.md"
    "THIRD_PARTY_NOTICES.md"
    "docs/provenance.md"
)
    set(document_path "${HOUIO_SOURCE_DIR}/${required_document}")
    if(NOT EXISTS "${document_path}")
        message(FATAL_ERROR "Required release-policy document is missing: ${document_path}")
    endif()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/LICENSE_STATUS.md" license_status)
string(FIND
    "${license_status}"
    "does not currently provide a project-wide license grant"
    license_warning_position
)
if(license_warning_position EQUAL -1)
    message(FATAL_ERROR "LICENSE_STATUS.md no longer states the project-wide license blocker")
endif()

file(READ
    "${HOUIO_SOURCE_DIR}/.github/workflows/bundled-scene-io.yml"
    bundled_workflow
)
foreach(forbidden_text IN ITEMS
    "actions/upload-artifact"
    "tags:"
)
    string(FIND "${bundled_workflow}" "${forbidden_text}" forbidden_position)
    if(NOT forbidden_position EQUAL -1)
        message(FATAL_ERROR
            "Bundled scene workflow reintroduced publishing behavior: ${forbidden_text}")
    endif()
endforeach()
string(FIND "${bundled_workflow}" "- master" master_branch_position)
if(master_branch_position EQUAL -1)
    message(FATAL_ERROR
        "Bundled scene workflow no longer seeds dependency caches from master")
endif()

file(READ "${HOUIO_SOURCE_DIR}/.github/workflows/ci.yml" ci_workflow)
foreach(forbidden_text IN ITEMS
    "Upload Houdini package"
    "houio-houdini-package-"
)
    string(FIND "${ci_workflow}" "${forbidden_text}" forbidden_position)
    if(NOT forbidden_position EQUAL -1)
        message(FATAL_ERROR
            "General CI reintroduced unlicensed package publication: ${forbidden_text}")
    endif()
endforeach()

file(READ
    "${HOUIO_SOURCE_DIR}/tools/houdini/build_houdini_package.py"
    houdini_package_builder
)
foreach(required_text IN ITEMS
    "LICENSE_STATUS_PATH"
    "THIRD_PARTY_NOTICES_PATH"
    "PROVENANCE_PATH"
)
    string(FIND "${houdini_package_builder}" "${required_text}" required_position)
    if(required_position EQUAL -1)
        message(FATAL_ERROR
            "Houdini package builder no longer carries legal metadata: ${required_text}")
    endif()
endforeach()
