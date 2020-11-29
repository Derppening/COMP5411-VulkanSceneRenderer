#include "application_bound.h"

application_bound::~application_bound() = default;

void application_bound::bind(VulkanExampleBase& app) {
  if (_app_) {
    throw std::runtime_error("application_bound::bind(): Application instance already bound");
  }

  _app_ = &app;
  setup(app);
}

void application_bound::unbind() {
  if (_app_) {
    destroy();
  }
  _app_ = nullptr;
}

VulkanExampleBase & application_bound::app() const {
  if (!_app_) {
    throw std::runtime_error("application_bound::app(): Application instance not bound");
  }
  return *_app_;
}
