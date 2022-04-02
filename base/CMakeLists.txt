file(GLOB BASE_SRC "*.cpp" "*.hpp" "*.h")
file(GLOB BASE_HEADERS "*.hpp" "*.h")

file(GLOB IMGUI_SRC "${imgui_SOURCE_DIR}/*.cpp")
add_library(imgui STATIC ${IMGUI_SRC})
target_include_directories(imgui SYSTEM PUBLIC
        ${imgui_SOURCE_DIR})

add_library(base STATIC ${BASE_SRC})
target_include_directories(base SYSTEM PUBLIC
        .)
target_link_libraries(base ${Vulkan_LIBRARIES} fmt ktx glfw imgui ${CMAKE_THREAD_LIBS_INIT})