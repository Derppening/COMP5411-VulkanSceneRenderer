#pragma once

#include <vulkan/vulkan.hpp>

#include "application_bound.h"

class tessellation : public application_bound {
 public:
  bool supported() const;
  bool enabled() const noexcept { return supported() && _mode_ > 0; }

  int& mode() noexcept { return _mode_; }
  float& level() noexcept { return _level_; }
  float& alpha() noexcept { return _alpha_; }

  void populate_ci(vk::GraphicsPipelineCreateInfo& create_info,
                   std::vector<vk::PipelineShaderStageCreateInfo>& shader_stages) const;

 protected:
  void setup(VulkanExampleBase& app) override;
  void destroy() override;

 private:
  int _mode_ = 0;
  float _level_ = 3.0f;
  float _alpha_ = 1.0f;

  struct {
    vk::ShaderModule _passthrough_ctrl_;
    vk::ShaderModule _passthrough_eval_;
    vk::ShaderModule _pn_ctrl_;
    vk::ShaderModule _pn_eval_;
  } _shader_modules_;
};
