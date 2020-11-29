#pragma once

#include "application_bound.h"

class normals_pipeline : public application_bound {
 public:
  void set_pipeline_layout(vk::PipelineLayout pipeline_layout);

  void create_pipeline();

  bool supported() const;
  bool enabled() const { return supported() && _length_ > 0.0f; }
  vk::SampleCountFlagBits& sample_count() noexcept { return _sample_count_; }
  bool& use_sample_shading() noexcept { return _use_sample_shading_; }
  float& length() noexcept { return _length_; }
  vk::Pipeline pipeline() const noexcept { return *_pipeline_; }

 protected:
  void setup(VulkanExampleBase &app) override;
  void destroy() override;

 private:
  vk::SampleCountFlagBits _sample_count_ = vk::SampleCountFlagBits::e1;
  bool _use_sample_shading_ = false;

  float _length_ = 0.0f;

  struct {
    vk::ShaderModule _vert;
    vk::ShaderModule _geom;
    vk::ShaderModule _frag;
  } _shader_modules_;

  vk::PipelineLayout _pipeline_layout_;
  vk::UniquePipeline _pipeline_;
};
