/*
* Vulkan texture loader
*
* Copyright(C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license(MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <fstream>
#include <stdlib.h>
#include <string>
#include <vector>

#include "vulkan/vulkan.h"

#include <ktx.h>
#include <ktxvulkan.h>

#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanTools.h"

namespace vks
{
class Texture
{
  public:
	vks::VulkanDevice *   device;
	vk::UniqueImage               image;
	vk::ImageLayout         imageLayout;
	vk::UniqueDeviceMemory        deviceMemory;
	vk::UniqueImageView           view;
	uint32_t              width, height;
	uint32_t              mipLevels;
	uint32_t              layerCount;
	vk::DescriptorImageInfo descriptor;
	vk::UniqueSampler             sampler;

	void      updateDescriptor();
	void      destroy();
	ktxResult loadKTXFile(std::string filename, ktxTexture **target);
};

class Texture2D : public Texture
{
  public:
	void loadFromFile(
	    std::string        filename,
	    vk::Format           format,
	    vks::VulkanDevice *device,
	    vk::Queue            copyQueue,
	    vk::ImageUsageFlags  imageUsageFlags = vk::ImageUsageFlagBits::eSampled,
	    vk::ImageLayout      imageLayout     = vk::ImageLayout::eShaderReadOnlyOptimal,
	    bool               forceLinear     = false);
	void fromBuffer(
	    void *             buffer,
	    vk::DeviceSize       bufferSize,
	    vk::Format           format,
	    uint32_t           texWidth,
	    uint32_t           texHeight,
	    vks::VulkanDevice *device,
	    vk::Queue            copyQueue,
	    vk::Filter           filter          = vk::Filter::eLinear,
	    vk::ImageUsageFlags  imageUsageFlags = vk::ImageUsageFlagBits::eSampled,
	    vk::ImageLayout      imageLayout     = vk::ImageLayout::eShaderReadOnlyOptimal);
};

class Texture2DArray : public Texture
{
  public:
	void loadFromFile(
	    std::string        filename,
	    vk::Format           format,
	    vks::VulkanDevice *device,
	    vk::Queue            copyQueue,
	    vk::ImageUsageFlags  imageUsageFlags = vk::ImageUsageFlagBits::eSampled,
	    vk::ImageLayout      imageLayout     = vk::ImageLayout::eShaderReadOnlyOptimal);
};

class TextureCubeMap : public Texture
{
  public:
	void loadFromFile(
	    std::string        filename,
	    vk::Format           format,
	    vks::VulkanDevice *device,
	    vk::Queue            copyQueue,
	    vk::ImageUsageFlags  imageUsageFlags = vk::ImageUsageFlagBits::eSampled,
	    vk::ImageLayout      imageLayout     = vk::ImageLayout::eShaderReadOnlyOptimal);
};
}        // namespace vks
