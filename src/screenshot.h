#pragma once

#include <chrono>
#include <string>

#include <vulkanexamplebase.h>

#include "application_bound.h"

class screenshot : public application_bound {
 public:
  void capture();

  bool show_save_message() const;
  const std::string& filename() const noexcept { return _filename_; }

 protected:
  void setup(VulkanExampleBase& app) override;
  void destroy() override;

 private:
  VulkanExampleBase* _app_ = nullptr;

  std::chrono::system_clock::time_point _save_time_;
  std::string _filename_;
};
