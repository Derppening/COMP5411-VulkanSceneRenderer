#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>

#include "application_bound.h"

class query_pool : public application_bound {
 public:
  void begin(vk::CommandBuffer command_buffer) const;
  void end(vk::CommandBuffer command_buffer) const;
  void reset(vk::CommandBuffer command_buffer) const;

  void update_query_results();

  bool enabled() const noexcept { return static_cast<bool>(_query_pool_); }
  const std::vector<std::uint64_t>& query_results() const;
  const std::vector<std::string>& pipeline_stat_names() const;

 protected:
  void setup(VulkanExampleBase &app) override;
  void destroy() override;

 private:
  vk::UniqueQueryPool _query_pool_;

  std::vector<std::string> _pipeline_stat_names_;
  std::vector<std::uint64_t> _query_results_;
};
