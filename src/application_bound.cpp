#include "application_bound.h"

application_bound::~application_bound() {
  if (_app_) {
    unbind();
  }
}

void application_bound::bind(VulkanExampleBase& app) {
  _app_ = &app;
  setup(app);
}

void application_bound::unbind() {
  destroy();
  _app_ = nullptr;
}

VulkanExampleBase & application_bound::app() {
  if (!_app_) {
    throw std::runtime_error("application_bound::app(): Application instance not bound");
  }
  return *_app_;
}
