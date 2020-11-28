#pragma once

#include <vulkan/vulkan.hpp>

#include "light_cube.h"
#include "light_ubo.h"
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
      glm::vec4 viewPos;
    } values;
  } shader_data;

  vk::UniquePipelineLayout pipeline_layout;
  vk::DescriptorSet descriptor_set;

  struct descriptor_set_layouts {
    vk::UniqueDescriptorSetLayout matrices;
    vk::UniqueDescriptorSetLayout textures;
  } descriptor_set_layouts;

  vulkan_scene_renderer();
  ~vulkan_scene_renderer() override;
  void getEnabledFeatures() override;
  void buildCommandBuffers() override;
  void setupRenderPass() override;
  void setupFrameBuffer() override;
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
  vk::SampleCountFlagBits _get_max_usable_sample_count();
  void _setup_multisample_target();
  glm::vec3 _calc_camera_direction();
  void _save_screenshot(const char* filename);
  void _update_sample_count(vk::SampleCountFlagBits sample_count, bool update_now = true);

  // Alignment required since boolean is just a int32_t
  struct alignas(4) _settings {
    bool blinnPhong = false;
  };

  ubo<_settings> _settings_ubo_;
  light_ubo _light_ubo_;

  bool _draw_light_ = false;
  bool _draw_scene_ = true;

  bool _wireframe_ = false;

  query_pool _query_pool_;
  light_cube _light_cube_;

  bool _use_sample_shading_ = false;
  std::vector<vk::SampleCountFlagBits> _supported_sample_counts_;
  vk::SampleCountFlagBits _sample_count_ = vk::SampleCountFlagBits::e1;
  int _sample_count_option_ = 0;

  struct {
    struct {
      vk::UniqueImage _image;
      vk::UniqueImageView _view;
      vk::UniqueDeviceMemory _memory;
    } _color;
    struct {
      vk::UniqueImage _image;
      vk::UniqueImageView _view;
      vk::UniqueDeviceMemory _memory;
    } _depth;
  } _multisample_target_;

  struct {
    float _length = 0.0f;

    vk::UniquePipeline _pipeline;
  } _gs_;

  struct {
    int _mode = 0;
    float _level = 3.0f;
    float _alpha = 1.0f;

    vk::ShaderModule _passthrough_module_tesc;
    vk::ShaderModule _passthrough_module_tese;
    vk::ShaderModule _pn_module_tesc;
    vk::ShaderModule _pn_module_tese;
  } _ts_;

  struct {
    bool _is_saved = false;
    std::chrono::system_clock::time_point _save_time;
    std::string _filename;
  } _screenshot_;
};
