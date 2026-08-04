if(NOT DEFINED HOUIO_SOURCE_DIR OR HOUIO_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "HOUIO_SOURCE_DIR is required")
endif()

function(strip_cpp_comments input_content output_variable)
    set(code_content "${input_content}")
    string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" code_content "${code_content}")
    string(REGEX REPLACE "//[^\r\n]*" "" code_content "${code_content}")
    set(${output_variable} "${code_content}" PARENT_SCOPE)
endfunction()

foreach(retired_path IN ITEMS
    "${HOUIO_SOURCE_DIR}/include/ttl"
    "${HOUIO_SOURCE_DIR}/include/houio/math/Half"
    "${HOUIO_SOURCE_DIR}/src/math/Half"
)
    if(EXISTS "${retired_path}")
        message(FATAL_ERROR "Retired third-party source tree was restored: ${retired_path}")
    endif()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/CMakeLists.txt" project_source)
foreach(retired_converter_pattern IN ITEMS
    "add_executable(houio_convert"
    "TARGET_FILE:houio_convert"
    "IN ITEMS houio_cli houio_convert"
)
    string(FIND "${project_source}" "${retired_converter_pattern}" converter_position)
    if(NOT converter_position EQUAL -1)
        message(FATAL_ERROR "Retired compatibility converter target was restored: ${retired_converter_pattern}")
    endif()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/houdini/package/houio.json.in" houdini_package_source)
string(FIND "${houdini_package_source}" "HOUIO_CONVERT_EXECUTABLE" converter_environment_position)
if(NOT converter_environment_position EQUAL -1)
    message(FATAL_ERROR "Retired compatibility converter environment variable was restored")
endif()

file(GLOB_RECURSE active_sources LIST_DIRECTORIES FALSE
    "${HOUIO_SOURCE_DIR}/include/houio/*.h"
    "${HOUIO_SOURCE_DIR}/include/houio/*.hpp"
    "${HOUIO_SOURCE_DIR}/src/*.cpp"
    "${HOUIO_SOURCE_DIR}/tests/*.cpp"
    "${HOUIO_SOURCE_DIR}/tools/*.cpp"
)
foreach(file_path IN LISTS active_sources)
    file(READ "${file_path}" file_content)
    strip_cpp_comments("${file_content}" code_content)
    string(REGEX MATCH "(^|[^A-Za-z0-9_])typedef([^A-Za-z0-9_]|$)" legacy_typedef "${code_content}")
    string(REGEX MATCH "(^|[^A-Za-z0-9_])NULL([^A-Za-z0-9_]|$)" legacy_null "${code_content}")
    if(legacy_typedef)
        message(FATAL_ERROR "Active source reintroduced typedef: ${file_path}")
    endif()
    if(legacy_null)
        message(FATAL_ERROR "Active source reintroduced NULL: ${file_path}")
    endif()

    file(STRINGS "${file_path}" source_lines)
    foreach(source_line IN LISTS source_lines)
        if(source_line MATCHES "^[ \t]*#[ \t]*if[ \t]+0([ \t]|$)")
            message(FATAL_ERROR "Active source reintroduced preprocessor-disabled implementation: ${file_path}: ${source_line}")
        endif()
        if(source_line MATCHES "^[ \t]*//[ \t]*(return[ \t]+(true|false|nullptr)|throw[ \t]+|delete[ \t]+|new[ \t]+|if[ \t]*\\(|for[ \t]*\\(|while[ \t]*\\()")
            message(FATAL_ERROR "Active source reintroduced commented-out implementation: ${file_path}: ${source_line}")
        endif()

        string(REGEX REPLACE "//.*$" "" code_line "${source_line}")
        if(code_line MATCHES "(^|[=({,:;])[ \t]*new[ \t]+[A-Za-z_:]")
            message(FATAL_ERROR "Active source reintroduced raw new allocation: ${file_path}: ${source_line}")
        endif()
        if(code_line MATCHES "(^|[;{})])[ \t]*delete[ \t]*(\\[\\])?[ \t]+")
            message(FATAL_ERROR "Active source reintroduced raw delete: ${file_path}: ${source_line}")
        endif()
        if(code_line MATCHES "(^|[^A-Za-z0-9_])(malloc|calloc|realloc|free)[ \t]*\\(")
            message(FATAL_ERROR "Active source reintroduced C heap ownership: ${file_path}: ${source_line}")
        endif()
    endforeach()

    string(FIND "${code_content}" "<ttl/" ttl_reference)
    string(FIND "${code_content}" "<houio/math/Half/" half_reference)
    if(NOT ttl_reference EQUAL -1)
        message(FATAL_ERROR "Active source references retired TTL code: ${file_path}")
    endif()
    if(NOT half_reference EQUAL -1)
        message(FATAL_ERROR "Active source references retired half code: ${file_path}")
    endif()

    foreach(opengl_pattern IN ITEMS
        "#include <GL/"
        "#include <OpenGL/"
        "GLuint"
        "GL_ARRAY_BUFFER"
        "glGenBuffers("
        "glDeleteBuffers("
        "glBindBuffer("
    )
        string(FIND "${code_content}" "${opengl_pattern}" opengl_reference)
        if(NOT opengl_reference EQUAL -1)
            message(FATAL_ERROR "Core source reintroduced OpenGL-specific buffer state ${opengl_pattern}: ${file_path}")
        endif()
    endforeach()
endforeach()

file(GLOB_RECURSE public_headers LIST_DIRECTORIES FALSE
    "${HOUIO_SOURCE_DIR}/include/houio/*.h"
    "${HOUIO_SOURCE_DIR}/include/houio/*.hpp"
)
foreach(header_path IN LISTS public_headers)
    file(READ "${header_path}" header_content)
    strip_cpp_comments("${header_content}" header_code)
    string(REPLACE "\r" "" header_code "${header_code}")
    string(REPLACE "\n" ";" header_lines "${header_code}")
    foreach(header_line IN LISTS header_lines)
        if(header_line MATCHES "^[ \t]*enum[ \t]+"
            AND NOT header_line MATCHES "^[ \t]*enum[ \t]+(class|struct)[ \t]+")
            message(FATAL_ERROR "Public header reintroduced an unscoped enum: ${header_path}: ${header_line}")
        endif()
    endforeach()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/include/houio/Field.h" field_header)
foreach(retired_name IN ITEMS
    "sample("
    "lvalue("
    "getResolution("
    "getVoxelSize("
    "static Ptr load("
    "void store("
    "void storeWithoutBoundingBox("
    "create(const typename Field<Source>::Ptr&"
)
    string(FIND "${field_header}" "${retired_name}" retired_position)
    if(NOT retired_position EQUAL -1)
        message(FATAL_ERROR "Field public API reintroduced retired name ${retired_name}")
    endif()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/include/houio/Geometry.h" geometry_header)
foreach(retired_name IN ITEMS
    "getBound("
    "hasAttr("
    "removeAttr("
    "numPrimitives("
    "numPrimitiveVertices("
    "merge(const std::vector<Ptr>&"
)
    string(FIND "${geometry_header}" "${retired_name}" retired_position)
    if(NOT retired_position EQUAL -1)
        message(FATAL_ERROR "Geometry public API reintroduced retired name ${retired_name}")
    endif()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/include/houio/Attribute.h" attribute_header)
foreach(retired_pattern IN ITEMS
    "static constexpr ComponentType INVALID"
    "static constexpr ComponentType INT"
    "static constexpr ComponentType FLOAT"
    "static constexpr ComponentType INT64"
    "static constexpr ComponentType HALF"
    "std::span<std::byte> bytes()"
    "std::span<std::byte> elementBytes("
    "componentType(const std::string"
)
    string(FIND "${attribute_header}" "${retired_pattern}" retired_position)
    if(NOT retired_position EQUAL -1)
        message(FATAL_ERROR "Attribute API reintroduced retired pattern ${retired_pattern}")
    endif()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/include/houio/HouGeoAdapter.h" adapter_header)
foreach(retired_pattern IN ITEMS
    "ATTR_TYPE_"
    "ATTR_STORAGE_"
    "getName("
    "getType("
    "getTupleSize("
    "getStorage("
    "getNumElements("
    "getString("
    "getDictionary("
    "storageSize("
    "virtual int tupleSize("
    "friend class ::houio::HouGeoIO"
)
    string(FIND "${adapter_header}" "${retired_pattern}" retired_position)
    if(NOT retired_position EQUAL -1)
        message(FATAL_ERROR "Adapter API reintroduced retired pattern ${retired_pattern}")
    endif()
endforeach()

foreach(vector_header IN ITEMS
    "${HOUIO_SOURCE_DIR}/include/houio/math/Vec2.h"
    "${HOUIO_SOURCE_DIR}/include/houio/math/Vec3.h"
    "${HOUIO_SOURCE_DIR}/include/houio/math/Vec4.h"
)
    file(READ "${vector_header}" vector_content)
    foreach(retired_name IN ITEMS
        "getLength("
        "getSquaredLength("
        "constexpr bool operator+=("
        "constexpr bool operator-=("
        "constexpr bool operator*=("
        "constexpr bool operator/=("
    )
        string(FIND "${vector_content}" "${retired_name}" retired_position)
        if(NOT retired_position EQUAL -1)
            message(FATAL_ERROR "Vector API reintroduced retired pattern ${retired_name}: ${vector_header}")
        endif()
    endforeach()
endforeach()

foreach(vector_algo IN ITEMS
    "${HOUIO_SOURCE_DIR}/include/houio/math/Vec2Algo.h"
    "${HOUIO_SOURCE_DIR}/include/houio/math/Vec3Algo.h"
    "${HOUIO_SOURCE_DIR}/include/houio/math/Vec4Algo.h"
)
    file(READ "${vector_algo}" vector_algo_content)
    foreach(retired_pattern IN ITEMS
        "dotProduct("
        "crossProduct("
        "nondominantAxis("
        "baseVec3("
        "baseVec4("
        "coordinateSystem("
        "#include <math.h>"
        "#include \"Vec2Algo.h\""
    )
        string(FIND "${vector_algo_content}" "${retired_pattern}" retired_position)
        if(NOT retired_position EQUAL -1)
            message(FATAL_ERROR "Vector algorithm reintroduced retired pattern ${retired_pattern}: ${vector_algo}")
        endif()
    endforeach()
endforeach()

foreach(matrix_header IN ITEMS
    "${HOUIO_SOURCE_DIR}/include/houio/math/Matrix22.h"
    "${HOUIO_SOURCE_DIR}/include/houio/math/Matrix33.h"
)
    file(READ "${matrix_header}" matrix_content)
    foreach(retired_name IN ITEMS
        "getDeterminant("
        "Zero("
        "Identity("
        "RotationMatrix("
    )
        string(FIND "${matrix_content}" "${retired_name}" retired_position)
        if(NOT retired_position EQUAL -1)
            message(FATAL_ERROR "Matrix API reintroduced retired name ${retired_name}: ${matrix_header}")
        endif()
    endforeach()
endforeach()

foreach(matrix_algo IN ITEMS
    "${HOUIO_SOURCE_DIR}/include/houio/math/Matrix22Algo.h"
    "${HOUIO_SOURCE_DIR}/include/houio/math/Matrix33Algo.h"
)
    file(READ "${matrix_algo}" matrix_algo_content)
    string(FIND "${matrix_algo_content}" "matrixMultiply(" retired_matrix_multiply)
    if(NOT retired_matrix_multiply EQUAL -1)
        message(FATAL_ERROR "Matrix algorithm reintroduced output-parameter multiplication: ${matrix_algo}")
    endif()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/include/houio/math/Matrix44.h" matrix44_header)
foreach(retired_name IN ITEMS
    "getRight("
    "getUp("
    "getDir("
    "getTranslation("
    "getOrientation("
    "getNormalizedOrientation("
    "getTransposed("
    "Zero("
    "Identity("
    "RotationMatrixX("
    "RotationMatrixY("
    "RotationMatrixZ("
    "RotationMatrix("
    "TranslationMatrix("
    "ScaleMatrix("
    "inverse()"
    "setDir("
    "multiplied("
)
    string(FIND "${matrix44_header}" "${retired_name}" retired_position)
    if(NOT retired_position EQUAL -1)
        message(FATAL_ERROR "Matrix44 API reintroduced retired name ${retired_name}")
    endif()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/include/houio/math/Matrix44Algo.h" matrix44_algo)
foreach(retired_name IN ITEMS
    "extractEulerXYZ("
    "extractAxisAngle("
    "matrixMultiply("
    "createLookAtMatrix("
    "createMatrixFromPolarCoordinates("
    "basisFromVector("
    "transformFromVector("
    "orthographicProjectionMatrix("
    "projectionMatrix("
    "constexpr void transform("
    "constexpr Matrix44<T>& transpose("
)
    string(FIND "${matrix44_algo}" "${retired_name}" retired_position)
    if(NOT retired_position EQUAL -1)
        message(FATAL_ERROR "Matrix44 algorithm reintroduced retired name ${retired_name}")
    endif()
endforeach()

foreach(bounds_header IN ITEMS
    "${HOUIO_SOURCE_DIR}/include/houio/math/BoundingBox2.h"
    "${HOUIO_SOURCE_DIR}/include/houio/math/BoundingBox3.h"
)
    file(READ "${bounds_header}" bounds_content)
    foreach(retired_name IN ITEMS "getCenter(" "isEmpty(" "makeEmpty(" "maxExtend(" "extendBy(")
        string(FIND "${bounds_content}" "${retired_name}" retired_position)
        if(NOT retired_position EQUAL -1)
            message(FATAL_ERROR "Bounding-box API reintroduced retired name ${retired_name}: ${bounds_header}")
        endif()
    endforeach()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/include/houio/math/Ray3.h" ray_header)
string(FIND "${ray_header}" "getPosition(" retired_ray_position)
if(NOT retired_ray_position EQUAL -1)
    message(FATAL_ERROR "Ray3 API reintroduced retired name getPosition(")
endif()

file(READ "${HOUIO_SOURCE_DIR}/include/houio/math/Color.h" color_header)
foreach(retired_name IN ITEMS
    "From255("
    "makeDWORD("
    "White("
    "Black("
    "Blue("
    "Yellow("
    "Green("
    "Red("
    "invert()"
)
    string(FIND "${color_header}" "${retired_name}" retired_position)
    if(NOT retired_position EQUAL -1)
        message(FATAL_ERROR "Color API reintroduced retired name ${retired_name}")
    endif()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/include/houio/math/Math.h" math_header)
foreach(retired_name IN ITEMS
    "getAlpha("
    "getRed("
    "getGreen("
    "getBlue("
    "setColor("
    "setRGBColor("
    "radToDeg("
    "degToRad("
    "#define MATH_"
    "float area("
    "distancePointPlane("
    "distancePointLine("
    "distancePointTriangle("
    "projectPointOnPlane("
    "projectPointOnLine("
    "mapValueToRange("
    "mapValueTo0_1("
    "Vec3f slerp("
    "float clamp("
    "inline T max("
    "inline T min("
    "inline T clamp("
    "sphericalToCartesian("
    "cartesianToSpherical("
    "bool quadratic("
)
    string(FIND "${math_header}" "${retired_name}" retired_position)
    if(NOT retired_position EQUAL -1)
        message(FATAL_ERROR "Math color API reintroduced retired name ${retired_name}")
    endif()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/include/houio/math/RNG.h" rng_header)
foreach(retired_pattern IN ITEMS
    "#define MATH_RNG_"
    "class RNG"
    "randomFloat("
    "randomUInt("
    "Seed("
    "g_randomNumber"
)
    string(FIND "${rng_header}" "${retired_pattern}" retired_position)
    if(NOT retired_position EQUAL -1)
        message(FATAL_ERROR "Random API reintroduced retired pattern ${retired_pattern}")
    endif()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/include/houio/json.h" json_header)
foreach(unscoped_pattern IN ITEMS "enum Type" "enum State")
    string(FIND "${json_header}" "${unscoped_pattern}" unscoped_position)
    if(NOT unscoped_position EQUAL -1)
        message(FATAL_ERROR "JSON API reintroduced unscoped enum ${unscoped_pattern}")
    endif()
endforeach()

file(GLOB test_sources LIST_DIRECTORIES FALSE "${HOUIO_SOURCE_DIR}/tests/*.cpp")
foreach(file_path IN LISTS test_sources)
    file(READ "${file_path}" test_content)
    string(FIND "${test_content}" "int fail(" local_failure_helper)
    if(NOT local_failure_helper EQUAL -1)
        message(FATAL_ERROR "Test reintroduced a private failure helper instead of TestSupport.h: ${file_path}")
    endif()
endforeach()

file(READ "${HOUIO_SOURCE_DIR}/CMakeLists.txt" root_cmake)
string(FIND "${root_cmake}" "src/math/Half/half.cpp" compiled_half_reference)
if(NOT compiled_half_reference EQUAL -1)
    message(FATAL_ERROR "The retired half implementation is still part of the build")
endif()
