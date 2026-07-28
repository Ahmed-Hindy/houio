# Compile the DSO at the language level selected by each Houdini SDK. HouIO's
# C++20 implementation is reached through NativePolygonWriter's C ABI, so no
# STL types cross this boundary. Remove only SideFX's /W1 because it would
# override this target's warnings policy on MSVC.
get_target_property(
    HOUIO_HOUDINI_INTERFACE_COMPILE_OPTIONS
    Houdini
    INTERFACE_COMPILE_OPTIONS
)
if(HOUIO_HOUDINI_INTERFACE_COMPILE_OPTIONS)
    if(MSVC)
        list(
            FILTER HOUIO_HOUDINI_INTERFACE_COMPILE_OPTIONS
            EXCLUDE REGEX "^/W[0-4]$"
        )
    endif()
    set_property(
        TARGET Houdini
        PROPERTY INTERFACE_COMPILE_OPTIONS
        "${HOUIO_HOUDINI_INTERFACE_COMPILE_OPTIONS}"
    )
endif()

add_library(
    houio_houdini_rop
    SHARED
    HoudiniGeometryAdapter.cpp
    SceneArchiveWriter.cpp
    ROP_HouIO.cpp
)

set(
    HOUIO_HOUDINI_DSOLIB_DIRECTORY
    "${HOUIO_HOUDINI_ROOT}/custom/houdini/dsolib"
)
file(
    GLOB
    HOUIO_HOUDINI_PYTHON_IMPORT_LIBRARIES
    "${HOUIO_HOUDINI_ROOT}/python*/libs/python*.lib"
)
list(
    FILTER HOUIO_HOUDINI_PYTHON_IMPORT_LIBRARIES
    EXCLUDE REGEX "[/\\\\]python3\\.lib$|stub\\.lib$"
)
list(LENGTH HOUIO_HOUDINI_PYTHON_IMPORT_LIBRARIES HOUIO_HOUDINI_PYTHON_LIBRARY_COUNT)
if(NOT HOUIO_HOUDINI_PYTHON_LIBRARY_COUNT EQUAL 1)
    message(FATAL_ERROR
        "Expected exactly one Houdini Python import library, found: "
        "${HOUIO_HOUDINI_PYTHON_IMPORT_LIBRARIES}")
endif()
list(GET HOUIO_HOUDINI_PYTHON_IMPORT_LIBRARIES 0 HOUIO_HOUDINI_PYTHON_IMPORT_LIBRARY)

set(HOUIO_HOUDINI_OPTIONAL_USD_LIBRARIES)
if(EXISTS "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_boost.lib")
    list(APPEND HOUIO_HOUDINI_OPTIONAL_USD_LIBRARIES
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_boost.lib")
endif()
if(EXISTS "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_python.lib")
    list(APPEND HOUIO_HOUDINI_OPTIONAL_USD_LIBRARIES
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_python.lib")
endif()
file(
    GLOB
    HOUIO_HOUDINI_BOOST_PYTHON_LIBRARIES
    "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/hboost_python*-mt-x64.lib"
)
list(APPEND
    HOUIO_HOUDINI_OPTIONAL_USD_LIBRARIES
    ${HOUIO_HOUDINI_BOOST_PYTHON_LIBRARIES}
)

target_link_libraries(
    houio_houdini_rop
    PRIVATE
        "$<LINK_ONLY:houio::houio>"
        Houdini
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/Alembic_sidefx.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_arch.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_ar.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_gf.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_js.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_kind.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_pcp.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_plug.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_sdf.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_tf.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_trace.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_usd.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_usdGeom.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_vt.lib"
        "${HOUIO_HOUDINI_DSOLIB_DIRECTORY}/libpxr_work.lib"
        ${HOUIO_HOUDINI_OPTIONAL_USD_LIBRARIES}
        "${HOUIO_HOUDINI_PYTHON_IMPORT_LIBRARY}"
)
target_include_directories(
    houio_houdini_rop
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        "${HOUIO_HOUDINI_ROOT}/toolkit/include/Imath"
)
target_compile_definitions(
    houio_houdini_rop
    PRIVATE
        HOUIO_VERSION_STRING="${PROJECT_VERSION}"
)
set_target_properties(
    houio_houdini_rop
    PROPERTIES
        CXX_EXTENSIONS OFF
        OUTPUT_NAME ROP_HouIO
)

if(MSVC)
    target_compile_options(
        houio_houdini_rop
        PRIVATE
            /W4
            /permissive-
            /Zc:__cplusplus
            /utf-8
            /wd4251
            /wd4275
            /wd4702
    )
    target_compile_definitions(
        houio_houdini_rop
        PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
    )
    if(HOUIO_WARNINGS_AS_ERRORS)
        target_compile_options(houio_houdini_rop PRIVATE /WX)
    endif()
else()
    target_compile_options(
        houio_houdini_rop
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
    )
    if(HOUIO_WARNINGS_AS_ERRORS)
        target_compile_options(houio_houdini_rop PRIVATE -Werror)
    endif()
endif()

houdini_configure_target(
    houio_houdini_rop
    INSTDIR "${CMAKE_CURRENT_BINARY_DIR}/dso"
)
