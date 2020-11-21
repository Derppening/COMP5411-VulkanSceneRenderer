#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>

class query_pool {
 public:
  void setup(vk::Device device, vk::PhysicalDeviceFeatures enabled_features);
  void destroy();

  void begin(vk::CommandBuffer command_buffer) const;
  void end(vk::CommandBuffer command_buffer) const;
  void reset(vk::CommandBuffer command_buffer) const;

  void update_query_results();

  const std::vector<std::uint64_t>& query_results() const;
  const std::vector<std::string>& pipeline_stat_names() const;

  static bool is_supported(vk::PhysicalDeviceFeatures features);

 private:
  vk::Device _logical_device_;
  vk::PhysicalDeviceFeatures _enabled_features_;
  vk::UniqueQueryPool _query_pool_;

  std::vector<std::string> _pipeline_stat_names_;
  std::vector<std::uint64_t> _query_results_;
};
