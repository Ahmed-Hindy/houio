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
    ROP_HouIO.cpp
)

target_link_libraries(
    houio_houdini_rop
    PRIVATE
        "$<LINK_ONLY:houio::houio>"
        Houdini
)
target_include_directories(
    houio_houdini_rop
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
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
