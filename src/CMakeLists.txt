add_executable(VulkanSceneRenderer
        main.cpp
        application_bound.cpp
        light_cube.cpp
        light_ubo.cpp
        normals_pipeline.cpp
        query_pool.cpp
        screenshot.cpp
        vulkan_gltf_scene.cpp)
target_compile_options(VulkanSceneRenderer PUBLIC ${CXX_WARN_FLAGS})
target_include_directories(VulkanSceneRenderer SYSTEM PUBLIC
        ${fmt_SOURCE_DIR}/include
        ${glm_SOURCE_DIR}
        ${ktx_SOURCE_DIR}/include
        ${ktx_SOURCE_DIR}/other_include
        ${Vulkan_INCLUDE_DIR})
target_link_libraries(VulkanSceneRenderer PUBLIC
        base
        ${Vulkan_LIBRARIES}
        fmt::fmt)