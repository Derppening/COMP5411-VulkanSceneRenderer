#pragma once

#include <vulkanexamplebase.h>

class application_bound {
 public:
  virtual ~application_bound();
  void bind(VulkanExampleBase& app);
  void unbind();

 protected:
  VulkanExampleBase& app();

  virtual void setup(VulkanExampleBase& app) = 0;
  virtual void destroy() = 0;

 private:
  VulkanExampleBase* _app_ = nullptr;
};
