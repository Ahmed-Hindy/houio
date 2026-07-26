if(NOT DEFINED HOUIO_CLI OR NOT EXISTS "${HOUIO_CLI}")
    message(FATAL_ERROR "HOUIO_CLI must reference the built houio executable")
endif()
if(NOT DEFINED HOUIO_TEST_DIRECTORY)
    message(FATAL_ERROR "HOUIO_TEST_DIRECTORY is required")
endif()

file(MAKE_DIRECTORY "${HOUIO_TEST_DIRECTORY}")

execute_process(
    COMMAND "${HOUIO_CLI}"
    RESULT_VARIABLE usage_result
    OUTPUT_VARIABLE usage_output
    ERROR_VARIABLE usage_error
)
if(NOT usage_result EQUAL 2)
    message(FATAL_ERROR
        "houio usage exit code changed: expected 2, got ${usage_result}\n"
        "stdout: ${usage_output}\nstderr: ${usage_error}")
endif()
string(FIND "${usage_output}" "Usage:" usage_position)
if(usage_position EQUAL -1)
    message(FATAL_ERROR "houio usage output no longer contains Usage:")
endif()

set(missing_input "${HOUIO_TEST_DIRECTORY}/missing.bgeo")
file(REMOVE "${missing_input}")
execute_process(
    COMMAND "${HOUIO_CLI}" validate "${missing_input}" --json
    RESULT_VARIABLE input_result
    OUTPUT_VARIABLE input_output
    ERROR_VARIABLE input_error
)
if(NOT input_result EQUAL 3)
    message(FATAL_ERROR
        "houio input exit code changed: expected 3, got ${input_result}\n"
        "stdout: ${input_output}\nstderr: ${input_error}")
endif()
string(FIND "${input_output}" "\"success\":false" input_success_position)
if(input_success_position EQUAL -1)
    message(FATAL_ERROR "houio missing-input JSON did not report success=false")
endif()

set(manifest "${HOUIO_TEST_DIRECTORY}/empty_manifest.json")
set(vdb_output "${HOUIO_TEST_DIRECTORY}/unsupported.vdb")
file(WRITE "${manifest}"
    "{\"schema\":\"houio.hom/1\","
    "\"point_count\":0,\"vertex_count\":0,\"primitive_count\":0,"
    "\"topology\":[],\"primitives\":[],"
    "\"attributes\":{"
    "\"point\":[{\"name\":\"P\",\"kind\":\"numeric\","
    "\"storage\":\"float32\",\"tuple_size\":4,"
    "\"element_count\":0,\"values\":[]}],"
    "\"vertex\":[],\"primitive\":[],\"global\":[]},"
    "\"groups\":{\"point\":{},\"vertex\":{},\"primitive\":{}}}"
)
file(REMOVE "${vdb_output}")
execute_process(
    COMMAND "${HOUIO_CLI}" write-manifest "${manifest}" "${vdb_output}" --json
    RESULT_VARIABLE unsupported_result
    OUTPUT_VARIABLE unsupported_output
    ERROR_VARIABLE unsupported_error
)
if(NOT unsupported_result EQUAL 5)
    message(FATAL_ERROR
        "houio unsupported exit code changed: expected 5, got ${unsupported_result}\n"
        "stdout: ${unsupported_output}\nstderr: ${unsupported_error}")
endif()
string(FIND "${unsupported_output}" "unsupported_input" unsupported_category_position)
if(unsupported_category_position EQUAL -1)
    message(FATAL_ERROR "houio unsupported JSON did not include unsupported_input")
endif()
if(EXISTS "${vdb_output}")
    message(FATAL_ERROR "houio created an unsupported VDB container output")
endif()

set(fragment_manifest "${HOUIO_TEST_DIRECTORY}/packed_fragment_manifest.json")
set(fragment_output "${HOUIO_TEST_DIRECTORY}/packed_fragment.bgeo")
file(WRITE "${fragment_manifest}"
    "{\"schema\":\"houio.hom/1\","
    "\"point_count\":1,\"vertex_count\":1,\"primitive_count\":1,"
    "\"topology\":[0],\"primitives\":[{"
    "\"type\":\"packed_fragment\",\"vertex_offset\":0,"
    "\"pivot\":[0,0,0],\"transform\":[1,0,0,0,1,0,0,0,1],"
    "\"fragment_attribute\":\"name\",\"fragment_name\":\"piece0\","
    "\"bounds\":[0,1,0,1,0,0],\"cached_bounds\":[0,1,0,1,0,0],"
    "\"embedded_manifest\":{\"schema\":\"houio.hom/1\","
    "\"point_count\":0,\"vertex_count\":0,\"primitive_count\":0,"
    "\"topology\":[],\"primitives\":[],\"attributes\":{"
    "\"point\":[],\"vertex\":[],\"primitive\":[],\"global\":[]}}}],"
    "\"attributes\":{\"point\":[{\"name\":\"P\",\"kind\":\"numeric\","
    "\"storage\":\"float32\",\"tuple_size\":4,\"element_count\":1,"
    "\"values\":[0,0,0,1]}],\"vertex\":[],\"primitive\":[],\"global\":[]}}"
)
file(REMOVE "${fragment_output}")
execute_process(
    COMMAND "${HOUIO_CLI}" write-manifest "${fragment_manifest}" "${fragment_output}" --json
    RESULT_VARIABLE fragment_write_result
    OUTPUT_VARIABLE fragment_write_output
    ERROR_VARIABLE fragment_write_error
)
if(NOT fragment_write_result EQUAL 0)
    message(FATAL_ERROR
        "houio failed to write packed-fragment manifest: ${fragment_write_result}\n"
        "stdout: ${fragment_write_output}\nstderr: ${fragment_write_error}")
endif()
execute_process(
    COMMAND "${HOUIO_CLI}" inspect "${fragment_output}" --json
    RESULT_VARIABLE fragment_inspect_result
    OUTPUT_VARIABLE fragment_inspect_output
    ERROR_VARIABLE fragment_inspect_error
)
if(NOT fragment_inspect_result EQUAL 0)
    message(FATAL_ERROR
        "houio failed to inspect packed-fragment output: ${fragment_inspect_result}\n"
        "stdout: ${fragment_inspect_output}\nstderr: ${fragment_inspect_error}")
endif()
string(FIND "${fragment_inspect_output}" "\"packed_fragment_records\":1" fragment_count_position)
if(fragment_count_position EQUAL -1)
    message(FATAL_ERROR
        "houio inspect did not report one packed fragment: ${fragment_inspect_output}")
endif()

set(disk_manifest "${HOUIO_TEST_DIRECTORY}/packed_disk_manifest.json")
set(disk_output "${HOUIO_TEST_DIRECTORY}/packed_disk.bgeo")
file(WRITE "${disk_manifest}"
    "{\"schema\":\"houio.hom/1\","
    "\"point_count\":1,\"vertex_count\":1,\"primitive_count\":1,"
    "\"topology\":[0],\"primitives\":[{"
    "\"type\":\"packed_disk\",\"vertex_offset\":0,"
    "\"filename\":\"$HIP/cache/payload.$F4.bgeo\","
    "\"expand_frame\":12.5,\"expand_filename\":true,"
    "\"pivot\":[0,0,0],\"transform\":[1,0,0,0,1,0,0,0,1],"
    "\"viewport_lod\":\"box\"}],"
    "\"attributes\":{\"point\":[{\"name\":\"P\",\"kind\":\"numeric\","
    "\"storage\":\"float32\",\"tuple_size\":4,\"element_count\":1,"
    "\"values\":[0,0,0,1]}],\"vertex\":[],\"primitive\":[],\"global\":[]}}"
)
file(REMOVE "${disk_output}")
execute_process(
    COMMAND "${HOUIO_CLI}" write-manifest "${disk_manifest}" "${disk_output}" --json
    RESULT_VARIABLE disk_write_result
    OUTPUT_VARIABLE disk_write_output
    ERROR_VARIABLE disk_write_error
)
if(NOT disk_write_result EQUAL 0)
    message(FATAL_ERROR
        "houio failed to write packed-disk manifest: ${disk_write_result}\n"
        "stdout: ${disk_write_output}\nstderr: ${disk_write_error}")
endif()
execute_process(
    COMMAND "${HOUIO_CLI}" inspect "${disk_output}" --json
    RESULT_VARIABLE disk_inspect_result
    OUTPUT_VARIABLE disk_inspect_output
    ERROR_VARIABLE disk_inspect_error
)
if(NOT disk_inspect_result EQUAL 0)
    message(FATAL_ERROR
        "houio failed to inspect packed-disk output: ${disk_inspect_result}\n"
        "stdout: ${disk_inspect_output}\nstderr: ${disk_inspect_error}")
endif()
string(FIND "${disk_inspect_output}" "\"packed_disk_records\":1" disk_count_position)
if(disk_count_position EQUAL -1)
    message(FATAL_ERROR
        "houio inspect did not report one packed disk: ${disk_inspect_output}")
endif()
