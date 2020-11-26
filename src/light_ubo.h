#pragma once

#include <algorithm>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <VulkanBuffer.h>
#include <VulkanDevice.h>

class light_ubo {
 public:
  struct settings {
    float dir_light_intensity = 1.0f;
    float point_light_intensity = 1.0f;
    float spot_light_intensity = 1.0f;
  };

  struct dir_light {
    alignas(16) glm::vec3 direction;

    float ambient;
    float diffuse;
    float specular;
  };

  struct point_light {
    alignas(16) glm::vec3 position;

    float constant;
    float linear;
    float quadratic;

    float ambient;
    float diffuse;
    float specular;
  };

  struct spot_light {
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 direction;
    float cutoff;
    float outer_cutoff;

    float constant;
    float linear;
    float quadratic;

    float ambient;
    float diffuse;
    float specular;
  };

  struct values {
    settings settings;
    dir_light dir_light = light_ubo::default_dir_light;
    point_light point_light = light_ubo::default_point_light;
    spot_light spot_light = light_ubo::default_spot_light;
  };

  light_ubo() = default;

  void prepare(vks::VulkanDevice& vulkan_device, bool update_now = true);
  void destroy();

  struct values& values() noexcept { return _values_; }
  void setup_descriptor_set_layout(vk::Device device, vk::ShaderStageFlags stage_flags);
  void setup_descriptor_sets(vk::Device device, vk::DescriptorPool descriptor_pool);

  void update();
  void update_distance(bool copy_ubo = true);
  void update_spot_light_radius(bool copy_ubo = true);

  vk::DescriptorSetLayout descriptor_set_layout() const { return *_descriptor_set_layout_; }
  vk::DescriptorSet descriptor_set() const { return _descriptor_set_; }
  int& point_light_distance() noexcept { return _point_light_distance_; }
  int& spot_light_distance() noexcept { return _spot_light_distance_; }
  float& spot_light_inner_radius() noexcept { return _spot_light_inner_radius_; }
  float& spot_light_outer_radius() noexcept { return _spot_light_outer_radius_; }

  void reset_dir_light();
  void reset_point_light();
  void reset_spot_light();

 private:
  static constexpr int _default_point_light_distance = 50;
  static constexpr int _default_spot_light_distance = 50;

  static constexpr float _default_spot_light_inner_radius = 12.5f;
  static constexpr float _default_spot_light_outer_radius = 17.5f;

  static const dir_light default_dir_light;
  static const point_light default_point_light;
  static const spot_light default_spot_light;

  static float _calc_linear_term(int dist);
  static float _calc_quad_term(int dist);

  struct values _values_ = {};

  vks::Buffer _buffer_;
  vk::UniqueDescriptorSetLayout _descriptor_set_layout_;
  vk::DescriptorSet _descriptor_set_;

  int _point_light_distance_ = _default_point_light_distance;
  int _spot_light_distance_ = _default_spot_light_distance;

  float _spot_light_inner_radius_ = _default_spot_light_inner_radius;
  float _spot_light_outer_radius_ = _default_spot_light_outer_radius;
};
