add_executable(VulkanSceneRenderer
#        maintest.cpp
#        VulkanglTFScene.cpp)
#        gltfscenerendering.cpp)
        main.cpp
        vulkan_gltf_scene.cpp)
target_compile_options(VulkanSceneRenderer PUBLIC ${CXX_WARN_FLAGS})
target_include_directories(VulkanSceneRenderer PUBLIC ${Vulkan_INCLUDE_DIR})
target_link_libraries(VulkanSceneRenderer PUBLIC
        base
        ${Vulkan_LIBRARIES}
        fmt::fmt
        glm::glm)