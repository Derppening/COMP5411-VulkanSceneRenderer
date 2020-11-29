#pragma once

#include "application_bound.h"

class multisample_target : public application_bound {
 public:
  vk::SampleCountFlagBits& sample_count() noexcept { return _sample_count_; }

  vk::Image image() const noexcept { return *_image_; }
  vk::ImageView view() const noexcept { return *_view_; }
  vk::DeviceMemory memory() const noexcept { return *_memory_; }

 protected:
  void destroy() final;

  vk::UniqueImage& image() noexcept { return _image_; }
  vk::UniqueImageView& view() noexcept { return _view_; }
  vk::UniqueDeviceMemory& memory() noexcept { return _memory_; }

 private:
  vk::SampleCountFlagBits _sample_count_;

  vk::UniqueImage _image_;
  vk::UniqueImageView _view_;
  vk::UniqueDeviceMemory _memory_;
};

class image_multisample_target : public multisample_target {
 protected:
  void setup(VulkanExampleBase& app) override;
};

class depth_multisample_target : public multisample_target {
 protected:
  void setup(VulkanExampleBase& app) override;
};
