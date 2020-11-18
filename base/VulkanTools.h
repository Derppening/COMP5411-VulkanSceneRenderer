/*
* Assorted Vulkan helper functions
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <vulkan/vulkan.hpp>
#include "VulkanInitializers.hpp"

#include <math.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <fstream>
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <optional>
#if defined(_WIN32)
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#endif

// Custom define for better code readability
#define VK_FLAGS_NONE 0
// Default fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 100000000000

// Macro to check and display Vulkan return results
#define VK_CHECK_RESULT(f)																				\
{																										\
	auto res = vk::Result((f));																			\
	if (res != vk::Result::eSuccess)																	\
	{																									\
		std::cout << "Fatal : VkResult is \"" << vks::tools::errorString(res) << "\" in " << __FILE__ << " at line " << __LINE__ << "\n"; \
		assert(res == vk::Result::eSuccess);															\
	}																									\
}

const std::string getAssetPath();

namespace vks
{
	namespace tools
	{
		/** @brief Disable message boxes on fatal errors */
		extern bool errorModeSilent;

		/** @brief Returns an error code as a string */
		std::string errorString(vk::Result errorCode);

		/** @brief Returns the device type as a string */
		std::string physicalDeviceTypeString(vk::PhysicalDeviceType type);

		// Selected a suitable supported depth format starting with 32 bit down to 16 bit
		// Returns false if none of the depth formats in the list is supported by the device
        std::optional<vk::Format> getSupportedDepthFormat(vk::PhysicalDevice physicalDevice);

		// Returns if a given format support LINEAR filtering
		vk::Bool32 formatIsFilterable(vk::PhysicalDevice physicalDevice, vk::Format format, vk::ImageTiling tiling);

		// Put an image memory barrier for setting an image layout on the sub resource into the given command buffer
		void setImageLayout(
			vk::CommandBuffer cmdbuffer,
			vk::Image image,
			vk::ImageLayout oldImageLayout,
			vk::ImageLayout newImageLayout,
			vk::ImageSubresourceRange subresourceRange,
			vk::PipelineStageFlags srcStageMask = vk::PipelineStageFlagBits::eAllCommands,
			vk::PipelineStageFlags dstStageMask = vk::PipelineStageFlagBits::eAllCommands);
		// Uses a fixed sub resource layout with first mip level and layer
		void setImageLayout(
			vk::CommandBuffer cmdbuffer,
			vk::Image image,
			vk::ImageAspectFlags aspectMask,
			vk::ImageLayout oldImageLayout,
			vk::ImageLayout newImageLayout,
			vk::PipelineStageFlags srcStageMask = vk::PipelineStageFlagBits::eAllCommands,
			vk::PipelineStageFlags dstStageMask = vk::PipelineStageFlagBits::eAllCommands);

		/** @brief Insert an image memory barrier into the command buffer */
		void insertImageMemoryBarrier(
			vk::CommandBuffer cmdbuffer,
			vk::Image image,
			vk::AccessFlags srcAccessMask,
			vk::AccessFlags dstAccessMask,
			vk::ImageLayout oldImageLayout,
			vk::ImageLayout newImageLayout,
			vk::PipelineStageFlags srcStageMask,
			vk::PipelineStageFlags dstStageMask,
			vk::ImageSubresourceRange subresourceRange);

		// Display error message and exit on fatal error
		void exitFatal(const std::string& message, int32_t exitCode);
		void exitFatal(const std::string& message, vk::Result resultCode);

		// Load a SPIR-V shader (binary)
		vk::UniqueShaderModule loadShader(const char *fileName, vk::Device device);

		/** @brief Checks if a file exists */
		bool fileExists(const std::string &filename);
	}
}
