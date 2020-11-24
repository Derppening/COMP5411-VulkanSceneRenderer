#pragma once

#include <vulkan/vulkan.hpp>

#include "light_cube.h"
#include "query_pool.h"
#include "ubo.h"
#include "vulkan_gltf_scene.h"
#include <vulkanexamplebase.h>

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

  /*struct {
    vks::Buffer buffer;
    struct {
      std::int32_t blinnPhong = 0;

      float minAmbientIntensity = 0.1f;

      std::int32_t treatAsPointLight = 0;
      float diffuseIntensity = 1.0f;
      float specularIntensity = 1.0f;
      float pointLightLinear = 0.22f;
      float pointLightQuad = 0.20f;

      int useFlashlight = 0;
      float flashlightCutoff = glm::cos(glm::radians(12.5f));
      float flashlightOuterCutoff = glm::cos(glm::radians(17.5f));
    } values;
    vk::UniqueDescriptorSetLayout descriptor_set_layout;
    vk::DescriptorSet descriptor_set;
  } settings_ubo;

  struct dir_light_ubo {
    vks::Buffer buffer;
    struct {
      float ambient = 0.05f;
      float diffuse = 0.4f;
      float specular = 0.5f;
    } values;
    vk::UniqueDescriptorSetLayout descriptor_set_layout;
    vk::DescriptorSet descriptor_set;

    void update();
  } dir_light_ubo;

  struct point_light_ubo {
    vks::Buffer buffer;
    struct {
      glm::vec3 position;
      float ambient = 0.05f;
      float diffuse = 0.8f;
      float specular = 1.0f;
      float linear = 0.22f;
      float quad = 0.20f;
    } values;
    vk::UniqueDescriptorSetLayout descriptor_set_layout;
    vk::DescriptorSet descriptor_set;

    void setup_descriptor_set_layout(vk::Device device);
    void setup_descriptor_set();
    void update();
  } point_light_ubo;*/

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
  void prepare() override;
  void render() override;
  void draw();
  void OnUpdateUIOverlay(vks::UIOverlay* overlay) override;

 private:
  struct _settings {
    std::int32_t blinnPhong = 0;

    float minAmbientIntensity = 0.1f;

    std::int32_t treatAsPointLight = 0;
    float diffuseIntensity = 1.0f;
    float specularIntensity = 1.0f;
    float pointLightLinear = 0.22f;
    float pointLightQuad = 0.20f;

    int useFlashlight = 0;
    float flashlightCutoff = glm::cos(glm::radians(12.5f));
    float flashlightOuterCutoff = glm::cos(glm::radians(17.5f));
  };

  void _update_point_light_values();

  ubo<_settings> _settings_ubo_;

  bool _wireframe_ = false;

  query_pool _query_pool_;
  light_cube _light_cube_;

  int _point_light_distance_ = 50;
};
