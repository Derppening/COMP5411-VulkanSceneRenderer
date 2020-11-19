if (USE_NATIVE_WSI)
    file(GLOB BASE_SRC "*.cpp" "*.hpp" "*.h" "${imgui_SOURCE_DIR}/*.cpp")
    file(GLOB BASE_HEADERS "*.hpp" "*.h")

    add_library(base STATIC ${BASE_SRC} ${KTX_SOURCES})
    target_include_directories(base PUBLIC
            .)
    target_include_directories(base SYSTEM PUBLIC
            ${imgui_SOURCE_DIR})

    if(WIN32)
        target_link_libraries(base ktx ${Vulkan_LIBRARY} ${WINLIBS})
     else(WIN32)
        target_link_libraries(base ktx ${Vulkan_LIBRARY} ${XCB_LIBRARIES} ${WAYLAND_CLIENT_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
    endif(WIN32)
else (USE_NATIVE_WSI)
    file(GLOB BASE_SRC "*.cpp" "*.hpp" "*.h")
    file(GLOB BASE_HEADERS "*.hpp" "*.h")

    file(GLOB IMGUI_SRC "${imgui_SOURCE_DIR}/*.cpp")
    add_library(imgui STATIC ${IMGUI_SRC})
    target_include_directories(imgui SYSTEM PUBLIC
            ${imgui_SOURCE_DIR})

    add_library(base STATIC ${BASE_SRC})
    target_include_directories(base SYSTEM PUBLIC
            .)
    target_link_libraries(base ${Vulkan_LIBRARIES} ktx glfw glm::glm imgui ${CMAKE_THREAD_LIBS_INIT})
endif (USE_NATIVE_WSI)