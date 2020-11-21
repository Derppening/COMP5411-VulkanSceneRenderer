add_executable(VulkanSceneRenderer
        main.cpp
        query_pool.cpp
        vulkan_gltf_scene.cpp)
target_compile_options(VulkanSceneRenderer PUBLIC ${CXX_WARN_FLAGS})
target_include_directories(VulkanSceneRenderer SYSTEM PUBLIC
        ${fmt_SOURCE_DIR}/include
        ${Vulkan_INCLUDE_DIR})
target_link_libraries(VulkanSceneRenderer PUBLIC
        base
        ${Vulkan_LIBRARIES}
        fmt::fmt
        glm::glm)