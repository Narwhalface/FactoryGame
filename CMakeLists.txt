cmake_minimum_required(VERSION 3.21)

project(FactoryGameHelloTriangle VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)

find_package(glfw3 3.3 CONFIG QUIET)
if(NOT TARGET glfw AND NOT TARGET glfw3::glfw)
    include(FetchContent)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(glfw)
endif()

if(TARGET glfw)
    set(GLFW_TARGET glfw)
elseif(TARGET glfw3::glfw)
    set(GLFW_TARGET glfw3::glfw)
else()
    message(FATAL_ERROR "A GLFW CMake target could not be found or created.")
endif()

find_program(GLSLC glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
find_program(GLSLANG_VALIDATOR glslangValidator HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")

if(GLSLC)
    set(SHADER_COMPILER "${GLSLC}")
    set(SHADER_FLAGS)
elseif(GLSLANG_VALIDATOR)
    set(SHADER_COMPILER "${GLSLANG_VALIDATOR}")
    set(SHADER_FLAGS -V)
else()
    message(FATAL_ERROR "No GLSL compiler found. Install the Vulkan SDK (glslc or glslangValidator is required).")
endif()

set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(SHADER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag"
)

set(SHADER_BINARIES)
foreach(SHADER_SOURCE IN LISTS SHADER_SOURCES)
    get_filename_component(SHADER_NAME "${SHADER_SOURCE}" NAME)
    set(SHADER_BINARY "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
    add_custom_command(
        OUTPUT "${SHADER_BINARY}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUTPUT_DIR}"
        COMMAND "${SHADER_COMPILER}" ${SHADER_FLAGS} "${SHADER_SOURCE}" -o "${SHADER_BINARY}"
        DEPENDS "${SHADER_SOURCE}"
        COMMENT "Compiling ${SHADER_NAME}"
        VERBATIM
    )
    list(APPEND SHADER_BINARIES "${SHADER_BINARY}")
endforeach()

add_custom_target(compile_shaders DEPENDS ${SHADER_BINARIES})

add_executable(hello_triangle src/main.cpp)
add_dependencies(hello_triangle compile_shaders)

target_link_libraries(hello_triangle PRIVATE Vulkan::Vulkan ${GLFW_TARGET})
target_compile_definitions(hello_triangle PRIVATE
    GLFW_INCLUDE_NONE
    HELLO_TRIANGLE_SHADER_DIR="${SHADER_OUTPUT_DIR}"
)

if(MSVC)
    target_compile_options(hello_triangle PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(hello_triangle PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_command(TARGET hello_triangle POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${SHADER_OUTPUT_DIR}"
            "$<TARGET_FILE_DIR:hello_triangle>/shaders"
    COMMENT "Copying shaders beside the executable"
)

set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT hello_triangle)
