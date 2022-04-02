cmake_minimum_required(VERSION 3.11)
project(VulkanSceneRenderer)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)

find_package(Threads)
find_package(Vulkan REQUIRED)

FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 7.1.2
        GIT_SHALLOW ON)
FetchContent_GetProperties(fmt)
if (NOT fmt_POPULATED)
    FetchContent_Populate(fmt)
    add_subdirectory(${fmt_SOURCE_DIR})
endif ()

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.2
        GIT_SHALLOW ON)
FetchContent_GetProperties(glfw)
if (NOT glfw_POPULATED)
    FetchContent_Populate(glfw)

    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

    add_subdirectory(${glfw_SOURCE_DIR})
endif ()

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.79
        GIT_SHALLOW ON)
FetchContent_GetProperties(imgui)
if (NOT imgui_POPULATED)
    FetchContent_Populate(imgui)

    include_directories(SYSTEM ${imgui_SOURCE_DIR})
endif ()

FetchContent_Declare(
        tinygltf
        GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
        GIT_TAG v2.5.0
        GIT_SHALLOW ON)
FetchContent_GetProperties(tinygltf)
if (NOT tinygltf_POPULATED)
    FetchContent_Populate(tinygltf)

    include_directories(SYSTEM ${tinygltf_SOURCE_DIR})
endif ()

FetchContent_Declare(
        ktx
        GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
        GIT_TAG v4.0.0-beta5
        GIT_SHALLOW ON)
FetchContent_GetProperties(ktx)
if (NOT ktx_POPULATED)
    FetchContent_Populate(ktx)

    set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)

    add_subdirectory(${ktx_SOURCE_DIR})
endif ()

include(cmake/compile_flags.cmake)

# Set preprocessor defines
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNOMINMAX -D_USE_MATH_DEFINES")

add_definitions(-DVK_EXAMPLE_DATA_DIR=\"${CMAKE_SOURCE_DIR}/data/\")

add_subdirectory(base)
add_subdirectory(src)
