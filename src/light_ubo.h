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
    float dir_light_intensity;
    float point_light_intensity;
    float spot_light_intensity;
  };

  struct dir_light {
    alignas(16) glm::vec4 direction;

    alignas(16) glm::vec4 ambient;
    alignas(16) glm::vec4 diffuse;
    alignas(16) glm::vec4 specular;
  };

  struct point_light {
    alignas(16) glm::vec4 position;

    float constant;
    float linear;
    float quadratic;

    alignas(16) glm::vec4 ambient;
    alignas(16) glm::vec4 diffuse;
    alignas(16) glm::vec4 specular;
  };

  struct spot_light {
    alignas(16) glm::vec4 position;
    alignas(16) glm::vec4 direction;
    float cutoff;
    float outer_cutoff;

    float constant;
    float linear;
    float quadratic;

    alignas(16) glm::vec4 ambient;
    alignas(16) glm::vec4 diffuse;
    alignas(16) glm::vec4 specular;
  };

  struct values {
    alignas(16) settings settings;
    alignas(16) dir_light dir_light;
    alignas(16) point_light point_light;
    alignas(16) spot_light spot_light;
  };

  light_ubo() = default;

  void prepare(vks::VulkanDevice& vulkan_device, bool update_now = true);
  void destroy();

  struct values& values() noexcept { return _values_; }
  void setup_descriptor_set_layout(vk::Device device, vk::ShaderStageFlags stage_flags);
  void setup_descriptor_sets(vk::Device device, vk::DescriptorPool descriptor_pool);

  void update();

  vk::DescriptorSetLayout descriptor_set_layout() const { return *_descriptor_set_layout_; }
  vk::DescriptorSet descriptor_set() const { return _descriptor_set_; }

 private:
  struct values _values_;

  vks::Buffer _buffer_;
  vk::UniqueDescriptorSetLayout _descriptor_set_layout_;
  vk::DescriptorSet _descriptor_set_;
};
