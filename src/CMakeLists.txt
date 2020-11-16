add_executable(VulkanSceneRenderer
        gltfscenerendering.cpp)
target_compile_options(VulkanSceneRenderer PUBLIC ${CXX_WARN_FLAGS})
target_include_directories(VulkanSceneRenderer PUBLIC ${Vulkan_INCLUDE_DIR})
target_link_libraries(VulkanSceneRenderer PUBLIC
        base
        ${Vulkan_LIBRARIES}
        fmt::fmt
        glm::glm)