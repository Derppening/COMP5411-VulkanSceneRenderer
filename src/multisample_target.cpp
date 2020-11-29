#include "multisample_target.h"

void multisample_target::destroy() {
  _memory_.reset();
  _view_.reset();
  _image_.reset();
}

void image_multisample_target::setup(VulkanExampleBase& app) {
  assert((app.deviceProperties.limits.framebufferColorSampleCounts >= sample_count()) &&
      (app.deviceProperties.limits.framebufferDepthSampleCounts >= sample_count()));

  // Color target
  auto info = vks::initializers::imageCreateInfo();
  info.imageType = vk::ImageType::e2D;
  info.format = app.swapChain.colorFormat;
  info.extent.width = app.width;
  info.extent.height = app.height;
  info.extent.depth = 1;
  info.mipLevels = 1;
  info.arrayLayers = 1;
  info.sharingMode = vk::SharingMode::eExclusive;
  info.tiling = vk::ImageTiling::eOptimal;
  info.samples = sample_count();
  // Image will only be used as a transient target
  info.usage = vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment;
  info.initialLayout = vk::ImageLayout::eUndefined;

  image() = app.device.createImageUnique(info);

  vk::MemoryRequirements mem_reqs = app.device.getImageMemoryRequirements(*image());
  auto mem_alloc = vks::initializers::memoryAllocateInfo();
  mem_alloc.allocationSize = mem_reqs.size;
  // We prefer a lazily allocated memory type
  // This means that the memory gets allocated when the implementation sees fit, e.g. when first using the images
  vk::Bool32 lazy_mem_type_present;
  mem_alloc.memoryTypeIndex = app.vulkanDevice->getMemoryType(mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eLazilyAllocated, &lazy_mem_type_present);
  if (!lazy_mem_type_present) {
    // If this is not available, fall back to device local memory
    mem_alloc.memoryTypeIndex = app.vulkanDevice->getMemoryType(mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
  }
  memory() = app.device.allocateMemoryUnique(mem_alloc);
  app.device.bindImageMemory(*image(), *memory(), 0);

  // Create image view for the MSAA target
  auto view_info = vks::initializers::imageViewCreateInfo();
  view_info.image = *image();
  view_info.viewType = vk::ImageViewType::e2D;
  view_info.format = app.swapChain.colorFormat;
  view_info.components.r = vk::ComponentSwizzle::eR;
  view_info.components.g = vk::ComponentSwizzle::eG;
  view_info.components.b = vk::ComponentSwizzle::eB;
  view_info.components.a = vk::ComponentSwizzle::eA;
  view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.layerCount = 1;

  view() = app.device.createImageViewUnique(view_info);
}

void depth_multisample_target::setup(VulkanExampleBase& app) {
  assert((app.deviceProperties.limits.framebufferColorSampleCounts >= sample_count()) &&
      (app.deviceProperties.limits.framebufferDepthSampleCounts >= sample_count()));

  // Depth target
  auto info = vks::initializers::imageCreateInfo();
  info.imageType = vk::ImageType::e2D;
  info.format = app.depthFormat;
  info.extent.width = app.width;
  info.extent.height = app.height;
  info.extent.depth = 1;
  info.mipLevels = 1;
  info.arrayLayers = 1;
  info.sharingMode = vk::SharingMode::eExclusive;
  info.tiling = vk::ImageTiling::eOptimal;
  info.samples = sample_count();
  // Image will only be used as a transient target
  info.usage = vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eDepthStencilAttachment;
  info.initialLayout = vk::ImageLayout::eUndefined;

  image() = app.device.createImageUnique(info);

  vk::MemoryRequirements mem_reqs = app.device.getImageMemoryRequirements(*image());
  auto mem_alloc = vks::initializers::memoryAllocateInfo();
  mem_alloc.allocationSize = mem_reqs.size;
  vk::Bool32 lazy_mem_type_present;
  mem_alloc.memoryTypeIndex = app.vulkanDevice->getMemoryType(mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eLazilyAllocated, &lazy_mem_type_present);
  if (!lazy_mem_type_present) {
    mem_alloc.memoryTypeIndex = app.vulkanDevice->getMemoryType(mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
  }

  memory() = app.device.allocateMemoryUnique(mem_alloc);
  app.device.bindImageMemory(*image(), *memory(), 0);

  // Create image view for the MSAA target
  auto view_info = vks::initializers::imageViewCreateInfo();
  view_info.image = *image();
  view_info.viewType = vk::ImageViewType::e2D;
  view_info.format = app.depthFormat;
  view_info.components.r = vk::ComponentSwizzle::eR;
  view_info.components.g = vk::ComponentSwizzle::eG;
  view_info.components.b = vk::ComponentSwizzle::eB;
  view_info.components.a = vk::ComponentSwizzle::eA;
  view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.layerCount = 1;

  view() = app.device.createImageViewUnique(view_info);
}
