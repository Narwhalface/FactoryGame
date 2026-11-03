cmake_minimum_required(VERSION 3.21)

project(FactoryGame VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)
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
set(GLFW_TARGET glfw)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
)
target_include_directories(imgui PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
)
target_link_libraries(imgui PUBLIC Vulkan::Vulkan ${GLFW_TARGET})

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
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/factory.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/factory.frag"
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

add_library(factory_core STATIC
    src/engine/renderer/Camera2D.cpp
    src/game/World.cpp
)
target_include_directories(factory_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_executable(factory_game
    src/main.cpp
    src/engine/Application.cpp
    src/engine/Window.cpp
    src/engine/renderer/Renderer.cpp
    src/engine/ui/DebugOverlay.cpp
)
add_dependencies(factory_game compile_shaders)

target_include_directories(factory_game PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(factory_game PRIVATE factory_core Vulkan::Vulkan ${GLFW_TARGET} imgui)
target_compile_definitions(factory_game PRIVATE
    GLFW_INCLUDE_NONE
    FACTORY_SHADER_DIR="${SHADER_OUTPUT_DIR}"
)

if(MSVC)
    target_compile_options(factory_core PRIVATE /W4 /permissive- /EHsc)
    target_compile_options(factory_game PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(factory_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(factory_game PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_command(TARGET factory_game POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${SHADER_OUTPUT_DIR}"
            "$<TARGET_FILE_DIR:factory_game>/shaders"
    COMMENT "Copying shaders beside the executable"
)

include(CTest)
if(BUILD_TESTING)
    add_executable(factory_core_tests tests/core_tests.cpp)
    target_link_libraries(factory_core_tests PRIVATE factory_core)
    if(MSVC)
        target_compile_options(factory_core_tests PRIVATE /W4 /permissive- /EHsc)
    else()
        target_compile_options(factory_core_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME factory_core_tests COMMAND factory_core_tests)
endif()

set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT factory_game)
