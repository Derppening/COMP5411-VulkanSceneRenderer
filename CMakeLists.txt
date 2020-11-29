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

if (USE_NATIVE_WSI)
    IF(UNIX AND NOT APPLE)
        set(LINUX TRUE)
    ENDIF()

    IF(WIN32)
        IF (NOT Vulkan_FOUND)
            find_library(Vulkan_LIBRARY NAMES vulkan-1 vulkan PATHS ${CMAKE_SOURCE_DIR}/libs/vulkan)
            IF (Vulkan_LIBRARY)
                set(Vulkan_FOUND ON)
                MESSAGE("Using bundled Vulkan library version")
            ENDIF()
        ENDIF()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_WIN32_KHR")
    ELSEIF(LINUX)
        IF (NOT Vulkan_FOUND)
            find_library(Vulkan_LIBRARY NAMES vulkan HINTS "$ENV{VULKAN_SDK}/lib" "${CMAKE_SOURCE_DIR}/libs/vulkan" REQUIRED)
            IF (Vulkan_LIBRARY)
                set(Vulkan_FOUND ON)
                MESSAGE("Using bundled Vulkan library version")
            ENDIF()
        ENDIF()
        find_package(Threads REQUIRED)
        IF(USE_D2D_WSI)
            MESSAGE("Using direct to display extension...")
            add_definitions(-D_DIRECT2DISPLAY)
        ELSEIF(USE_DIRECTFB_WSI)
            find_package(DirectFB REQUIRED)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_DIRECTFB_EXT")
            include_directories(${DIRECTFB_INCLUDE_DIR})
        ELSEIF(USE_WAYLAND_WSI)
            find_program(PKG_CONFIG pkg-config)
            if (NOT PKG_CONFIG)
                message(FATAL_ERROR "pkg-config binary not found")
            endif ()
            find_package(Wayland REQUIRED)
            if (NOT WAYLAND_FOUND)
                message(FATAL_ERROR "Wayland development package not found")
            endif ()
            pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
            if (NOT WAYLAND_PROTOCOLS_FOUND)
                message(FATAL_ERROR "Wayland protocols package not found")
            endif ()
            find_program(WAYLAND_SCANNER wayland-scanner)
            if (NOT WAYLAND_SCANNER)
                message(FATAL_ERROR "wayland-scanner binary not found")
            endif ()
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_WAYLAND_KHR")
            include_directories(${WAYLAND_INCLUDE_DIR})
            execute_process(COMMAND ${PKG_CONFIG} --variable=pkgdatadir wayland-protocols OUTPUT_VARIABLE protocol_dir OUTPUT_STRIP_TRAILING_WHITESPACE)
            execute_process(COMMAND ${WAYLAND_SCANNER} client-header ${protocol_dir}/stable/xdg-shell/xdg-shell.xml ${CMAKE_BINARY_DIR}/xdg-shell-client-protocol.h
                    COMMAND ${WAYLAND_SCANNER} private-code ${protocol_dir}/stable/xdg-shell/xdg-shell.xml ${CMAKE_BINARY_DIR}/xdg-shell-protocol.c)
            include_directories(${CMAKE_BINARY_DIR})
        ELSE(USE_D2D_WSI)
            find_package(XCB REQUIRED)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_XCB_KHR")
        ENDIF(USE_D2D_WSI)
    ELSEIF(APPLE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_MACOS_MVK -DVK_EXAMPLE_XCODE_GENERATED")
    ENDIF(WIN32)

    # Clang specific stuff
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-switch-enum")
    endif()

    # Compiler specific stuff
    IF(MSVC)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
    ELSEIF(APPLE)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fobjc-arc -xobjective-c++")
    ENDIF(MSVC)

    IF(WIN32)
        # Nothing here (yet)
    ELSEIF(APPLE)
        link_libraries(${Vulkan_LIBRARY} "-framework AppKit" "-framework QuartzCore")
    ELSE(WIN32)
        link_libraries(${XCB_LIBRARIES} ${Vulkan_LIBRARY} ${Vulkan_LIBRARY} ${DIRECTFB_LIBRARIES} ${WAYLAND_CLIENT_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
    ENDIF(WIN32)
endif (USE_NATIVE_WSI)

# Set preprocessor defines
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNOMINMAX -D_USE_MATH_DEFINES")

add_definitions(-DVK_EXAMPLE_DATA_DIR=\"${CMAKE_SOURCE_DIR}/data/\")

add_subdirectory(base)
add_subdirectory(src)
