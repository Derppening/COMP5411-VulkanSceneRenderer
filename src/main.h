#pragma once

#include <vulkan/vulkan.hpp>

#include "light_cube.h"
#include "query_pool.h"
#include "vulkan_gltf_scene.h"
#include "vulkanexamplebase.h"

class vulkan_scene_renderer : public VulkanExampleBase {
 public:
  vulkan_gltf_scene gltf_scene;

  struct shader_data {
    vks::Buffer buffer;
    struct values {
      glm::mat4 projection;
      glm::mat4 view;
      glm::vec4 lightPos = glm::vec4{0.0f, 2.5f, 0.0f, 1.0f};
      glm::vec4 viewPos;
    } values;
  } shader_data;

  vk::UniquePipelineLayout pipeline_layout;
  vk::DescriptorSet descriptor_set;

  struct descriptor_set_layouts {
    vk::UniqueDescriptorSetLayout matrices;
    vk::UniqueDescriptorSetLayout textures;
  } descriptor_set_layouts;

  struct {
    vks::Buffer buffer;
    struct {
      bool blinnPhong = false;
    } values;
    vk::UniqueDescriptorSetLayout descriptor_set_layout;
    vk::DescriptorSet descriptor_set;
  } settings_ubo;

  vulkan_scene_renderer();
  ~vulkan_scene_renderer() override;
  void getEnabledFeatures() override;
  void buildCommandBuffers() override;
  void load_gltf_file(std::string filename);
  void load_assets();
  void setup_descriptors();
  void prepare_pipelines();
  void prepare_uniform_buffers();
  void update_uniform_buffers();
  void update_settings_ubo();
  void prepare() override;
  void render() override;
  void draw();
  void OnUpdateUIOverlay(vks::UIOverlay* overlay) override;

 private:
  bool _wireframe_ = false;

  query_pool _query_pool_;
  light_cube _light_cube_;
};
