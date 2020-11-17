if (USE_NATIVE_WSI)
    file(GLOB BASE_SRC "*.cpp" "*.hpp" "*.h" "${imgui_SOURCE_DIR}/*.cpp")
    file(GLOB BASE_HEADERS "*.hpp" "*.h")

    set(KTX_DIR ${ktx_SOURCE_DIR})
    set(KTX_SOURCES
            ${KTX_DIR}/lib/texture.c
            ${KTX_DIR}/lib/hashlist.c
            ${KTX_DIR}/lib/checkheader.c
            ${KTX_DIR}/lib/swap.c
            ${KTX_DIR}/lib/memstream.c
            ${KTX_DIR}/lib/filestream.c)

    add_library(base STATIC ${BASE_SRC} ${KTX_SOURCES})
    target_include_directories(base PUBLIC
            .)
    target_include_directories(base SYSTEM PUBLIC
            ${imgui_SOURCE_DIR}
            ${KTX_DIR}/include
            ${KTX_DIR}/other_include)

    if(WIN32)
        target_link_libraries(base ${Vulkan_LIBRARY} ${WINLIBS})
     else(WIN32)
        target_link_libraries(base ${Vulkan_LIBRARY} ${XCB_LIBRARIES} ${WAYLAND_CLIENT_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
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