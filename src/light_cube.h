#pragma once

#include <array>

#include <vulkanexamplebase.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include "application_bound.h"
#include "ubo.h"

class light_cube : public application_bound {
 public:
  explicit light_cube(glm::mat4 projection = glm::mat4{}, glm::mat4 model = glm::mat4{}, glm::mat4 view = glm::mat4{});

  vk::SampleCountFlagBits& sample_count() noexcept { return _sample_count_; }
  bool& wireframe() { return _wireframe_; }
  glm::mat4& projection() { return _ubo_.values().projection; }
  glm::mat4& model() { return _ubo_.values().model; }
  glm::mat4& view() { return _ubo_.values().view; }
  glm::vec3& color() { return _push_consts_.color; }

  void prepare_pipeline();
  void draw(vk::CommandBuffer command_buffer);
  void update_uniform_buffers();

 protected:
  void setup(VulkanExampleBase& app) override;
  void destroy() override;

 private:
  static inline std::array<std::uint16_t, 6 * 6> _cube_indices = {
      0,
      3,
      6,
      6,
      2,
      0,

      1,
      5,
      7,
      7,
      4,
      1,

      4,
      2,
      0,
      0,
      1,
      4,

      7,
      6,
      3,
      3,
      5,
      7,

      0,
      3,
      5,
      5,
      1,
      0,

      2,
      6,
      7,
      7,
      4,
      2,
  };

  static inline std::array<float, 8 * 3> _cube_vertices = {
      -0.5f, -0.5f, -0.5f,
      -0.5f, -0.5f, 0.5f,
      -0.5f, 0.5f, -0.5f,
      0.5f, -0.5f, -0.5f,
      -0.5f, 0.5f, 0.5f,
      0.5f, -0.5f, 0.5f,
      0.5f, 0.5f, -0.5f,
      0.5f, 0.5f, 0.5f,
 };

  bool _wireframe_ = false;
  vk::SampleCountFlagBits _sample_count_;

  void _setup_descriptor_set_layout();
  void _setup_descriptor_pool();
  void _setup_descriptor_set();

  vks::Buffer _vertex_buffer_;
  vks::Buffer _index_buffer_;

  struct mvp {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
  };

  ubo<mvp> _ubo_;

  struct push_consts {
    glm::vec3 color = glm::vec3{1.0, 1.0, 1.0};
  } _push_consts_;

  vk::UniquePipeline _pipeline_;
  vk::UniquePipelineLayout _pipeline_layout_;
  vk::UniqueDescriptorPool _descriptor_pool_;
};
