file(GLOB BASE_SRC "*.cpp" "*.hpp" "*.h" "${imgui_SOURCE_DIR}/*.cpp")
file(GLOB BASE_HEADERS "*.hpp" "*.h")

add_library(base STATIC ${BASE_SRC})
target_include_directories(base SYSTEM PUBLIC
        .
        ${imgui_SOURCE_DIR})
target_link_libraries(base ktx)
if(WIN32)
    target_link_libraries(base ${Vulkan_LIBRARY} ${WINLIBS})
 else(WIN32)
    target_link_libraries(base ${Vulkan_LIBRARY} ${XCB_LIBRARIES} ${WAYLAND_CLIENT_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
endif(WIN32)