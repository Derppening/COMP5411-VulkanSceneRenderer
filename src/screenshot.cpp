#include "screenshot.h"

#include <fmt/format.h>

void screenshot::setup(VulkanExampleBase& app) {
  _app_ = &app;
}

void screenshot::destroy() {
  // no-op
}

void screenshot::capture() {
  if (!_app_) {
    throw std::runtime_error("screenshot::capture(): Application not bound to screenshot instance");
  }
  auto& app = *_app_;

  const auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
  _filename_ = fmt::format("{}.ppm", now.time_since_epoch().count());

  bool supports_blit = true;

  // Check blit support for source and destination
  vk::FormatProperties format_props;

  // Check if the device supports blitting from optimal images (the swapchain images are in optimal format)
  format_props = app.physicalDevice.getFormatProperties(app.swapChain.colorFormat);
  if (!(format_props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc)) {
    fmt::print(stderr, "Device does not support blitting from optimal tiled images, using copy instead of blit!\n");
    supports_blit = false;
  }

  // Check if the device supports blitting to linear images
  format_props = app.physicalDevice.getFormatProperties(vk::Format::eR8G8B8A8Unorm);
  if (!(format_props.linearTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst)) {
    fmt::print(stderr, "Device does not support blitting to linear tiled images, using copy instead of blit!\n");
    supports_blit = false;
  }

  // Source for the copy is the last rendered swapchain image
  vk::Image src_image = app.swapChain.images[app.currentBuffer];

  // Create the linear tiled destination image to copy to and to read the memory from
  auto image_create_ci = vks::initializers::imageCreateInfo();
  image_create_ci.imageType = vk::ImageType::e2D;
  // Note that vkCmdBlitImage (if supported) will also do format conversions if the swapchain color format would differ
  image_create_ci.format = vk::Format::eR8G8B8A8Unorm;
  image_create_ci.extent.width = app.width;
  image_create_ci.extent.height = app.height;
  image_create_ci.extent.depth = 1;
  image_create_ci.arrayLayers = 1;
  image_create_ci.mipLevels = 1;
  image_create_ci.initialLayout = vk::ImageLayout::eUndefined;
  image_create_ci.samples = vk::SampleCountFlagBits::e1;
  image_create_ci.tiling = vk::ImageTiling::eLinear;
  image_create_ci.usage = vk::ImageUsageFlagBits::eTransferDst;
  // Create the image
  vk::UniqueImage dst_image = app.device.createImageUnique(image_create_ci);
  // Create memory to back up the image
  auto mem_requirements = app.device.getImageMemoryRequirements(*dst_image);
  auto mem_alloc_info = vks::initializers::memoryAllocateInfo();
  mem_alloc_info.allocationSize = mem_requirements.size;
  // Memory must be host visible to copy from
  mem_alloc_info.memoryTypeIndex = app.vulkanDevice->getMemoryType(mem_requirements.memoryTypeBits,
                                                               vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
  vk::UniqueDeviceMemory dst_image_memory = app.device.allocateMemoryUnique(mem_alloc_info);
  app.device.bindImageMemory(*dst_image, *dst_image_memory, 0);

  // Do the actual blit from the swapchain image to our host visible destination image
  vk::UniqueCommandBuffer copy_cmd = app.vulkanDevice->createCommandBuffer(vk::CommandBufferLevel::ePrimary, true);

  // Transition destination image to transfer destination layout
  vks::tools::insertImageMemoryBarrier(
      *copy_cmd,
      *dst_image,
      {},
      vk::AccessFlagBits::eTransferWrite,
      vk::ImageLayout::eUndefined,
      vk::ImageLayout::eTransferDstOptimal,
      vk::PipelineStageFlagBits::eTransfer,
      vk::PipelineStageFlagBits::eTransfer,
      vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

  // Transition swapchain image from present to transfer source layout
  vks::tools::insertImageMemoryBarrier(
      *copy_cmd,
      src_image,
      vk::AccessFlagBits::eMemoryRead,
      vk::AccessFlagBits::eTransferRead,
      vk::ImageLayout::ePresentSrcKHR,
      vk::ImageLayout::eTransferSrcOptimal,
      vk::PipelineStageFlagBits::eTransfer,
      vk::PipelineStageFlagBits::eTransfer,
      vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

  // If source and destination support blit we'll blit as this also does automatic format conversion (e.g. from BGR to RGB)
  if (supports_blit) {
    // Define the region to blit (we will blit the whole swapchain image)
    vk::Offset3D blit_size;
    blit_size.x = static_cast<std::int32_t>(app.width);
    blit_size.y = static_cast<std::int32_t>(app.height);
    blit_size.z = 1;
    vk::ImageBlit image_blit_region{};
    image_blit_region.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    image_blit_region.srcSubresource.layerCount = 1;
    image_blit_region.srcOffsets[1] = blit_size;
    image_blit_region.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    image_blit_region.dstSubresource.layerCount = 1;
    image_blit_region.dstOffsets[1] = blit_size;

    // Issue the blit command
    copy_cmd->blitImage(
        src_image,
        vk::ImageLayout::eTransferSrcOptimal,
        *dst_image,
        vk::ImageLayout::eTransferDstOptimal,
        {image_blit_region},
        vk::Filter::eNearest);
  } else {
    // Otherwise use image copy (requires us to manually flip components)
    vk::ImageCopy image_copy_region{};
    image_copy_region.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    image_copy_region.srcSubresource.layerCount = 1;
    image_copy_region.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    image_copy_region.dstSubresource.layerCount = 1;
    image_copy_region.extent.width = app.width;
    image_copy_region.extent.height = app.height;
    image_copy_region.extent.depth = 1;

    // Issue the copy command
    copy_cmd->copyImage(
        src_image,
        vk::ImageLayout::eTransferSrcOptimal,
        *dst_image,
        vk::ImageLayout::eTransferDstOptimal,
        {image_copy_region});
  }

  // Transition destination image to general layout, which is the required layout for mapping the image memory later on
  vks::tools::insertImageMemoryBarrier(
      *copy_cmd,
      *dst_image,
      vk::AccessFlagBits::eTransferWrite,
      vk::AccessFlagBits::eMemoryRead,
      vk::ImageLayout::eTransferDstOptimal,
      vk::ImageLayout::eGeneral,
      vk::PipelineStageFlagBits::eTransfer,
      vk::PipelineStageFlagBits::eTransfer,
      vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

  // Transition back the swap chain image after the blit is done
  vks::tools::insertImageMemoryBarrier(
      *copy_cmd,
      src_image,
      vk::AccessFlagBits::eTransferRead,
      vk::AccessFlagBits::eMemoryRead,
      vk::ImageLayout::eTransferSrcOptimal,
      vk::ImageLayout::ePresentSrcKHR,
      vk::PipelineStageFlagBits::eTransfer,
      vk::PipelineStageFlagBits::eTransfer,
      vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

  app.vulkanDevice->flushCommandBuffer(copy_cmd, app.queue);

  // Get layout of the image (including row pitch)
  vk::ImageSubresource subresource{vk::ImageAspectFlagBits::eColor, 0, 0};
  vk::SubresourceLayout subresource_layout = app.device.getImageSubresourceLayout(*dst_image, subresource);

  // Map image memory so we can start copying from it
  const std::byte* data = static_cast<std::byte*>(app.device.mapMemory(*dst_image_memory, 0, VK_WHOLE_SIZE, {}));

  std::ofstream file{_filename_, std::ios::out | std::ios::binary};

  // ppm header
  file << fmt::format("P6\n{}\n{}\n255\n", app.width, app.height);

  // If source is BGR (destination is always RGB) and we can't use blit (which does automatic conversion), we'll have to manually swizzle color components
  bool color_swizzle = false;
  // Check if source is BGR
  // Note: Not complete, only contains most common and basic BGR surface formats for demonstration purposes
  if (!supports_blit) {
    auto formats_bgr = std::vector{
        vk::Format::eB8G8R8A8Srgb,
        vk::Format::eB8G8R8A8Unorm,
        vk::Format::eB8G8R8A8Snorm
    };
    color_swizzle = (std::find(formats_bgr.begin(), formats_bgr.end(), app.swapChain.colorFormat) != formats_bgr.end());
  }

  // ppm binary pixel data
  for (std::uint32_t y = 0; y < app.height; ++y) {
    auto row = reinterpret_cast<const unsigned int*>(data);
    for (std::uint32_t x = 0; x < app.width; ++x) {
      if (color_swizzle) {
        file.write(reinterpret_cast<const char*>(row + 2), 1);
        file.write(reinterpret_cast<const char*>(row + 1), 1);
        file.write(reinterpret_cast<const char*>(row), 1);
      } else {
        file.write(reinterpret_cast<const char*>(row), 3);
      }
      ++row;
    }
    data += subresource_layout.rowPitch;
  }
  file.close();

  fmt::print("Screenshot saved to disk\n");

  // Clean up resources
  app.device.unmapMemory(*dst_image_memory);
  dst_image_memory.reset();
  dst_image.reset();

  _save_time_ = now;
}

bool screenshot::show_save_message() const {
  const auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
  return now - _save_time_ < std::chrono::seconds{5};
}
