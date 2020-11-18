/*
* Assorted commonly used Vulkan helper functions
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "VulkanTools.h"

const std::string getAssetPath()
{
#if defined(VK_EXAMPLE_DATA_DIR)
	return VK_EXAMPLE_DATA_DIR;
#else
	return "./../data/";
#endif
}

namespace vks
{
	namespace tools
	{
		bool errorModeSilent = false;

		std::string errorString(vk::Result errorCode)
		{
		    return vk::to_string(errorCode);
		}

		std::string physicalDeviceTypeString(vk::PhysicalDeviceType type)
		{
		    return vk::to_string(type);
		}

		std::optional<vk::Format> getSupportedDepthFormat(vk::PhysicalDevice physicalDevice)
		{
			// Since all depth formats may be optional, we need to find a suitable depth format to use
			// Start with the highest precision packed format
			std::vector<vk::Format> depthFormats = {
				vk::Format::eD32SfloatS8Uint,
				vk::Format::eD32Sfloat,
				vk::Format::eD24UnormS8Uint,
				vk::Format::eD16UnormS8Uint,
				vk::Format::eD16Unorm
			};

			for (auto& format : depthFormats)
			{
				vk::FormatProperties formatProps = physicalDevice.getFormatProperties(format);
				// Format must support depth stencil attachment for optimal tiling
				if (formatProps.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
				{
					return std::make_optional(format);
				}
			}

			return std::nullopt;
		}

		// Returns if a given format support LINEAR filtering
		vk::Bool32 formatIsFilterable(vk::PhysicalDevice physicalDevice, vk::Format format, vk::ImageTiling tiling)
		{
			vk::FormatProperties formatProps = physicalDevice.getFormatProperties(format);

			if (tiling == vk::ImageTiling::eOptimal)
				return vk::Bool32{formatProps.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear};

			if (tiling == vk::ImageTiling::eLinear)
				return vk::Bool32{formatProps.linearTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear};

			return false;
		}

		// Create an image memory barrier for changing the layout of
		// an image and put it into an active command buffer
		// See chapter 11.4 "Image Layout" for details

		void setImageLayout(
			vk::CommandBuffer cmdbuffer,
			vk::Image image,
			vk::ImageLayout oldImageLayout,
			vk::ImageLayout newImageLayout,
			vk::ImageSubresourceRange subresourceRange,
			vk::PipelineStageFlags srcStageMask,
			vk::PipelineStageFlags dstStageMask)
		{
			// Create an image barrier object
			vk::ImageMemoryBarrier imageMemoryBarrier = vks::initializers::imageMemoryBarrier();
			imageMemoryBarrier.oldLayout = oldImageLayout;
			imageMemoryBarrier.newLayout = newImageLayout;
			imageMemoryBarrier.image = image;
			imageMemoryBarrier.subresourceRange = subresourceRange;

			// Source layouts (old)
			// Source access mask controls actions that have to be finished on the old layout
			// before it will be transitioned to the new layout
			switch (oldImageLayout)
			{
			case vk::ImageLayout::eUndefined:
				// Image layout is undefined (or does not matter)
				// Only valid as initial layout
				// No flags required, listed only for completeness
				imageMemoryBarrier.srcAccessMask = {};
				break;

			case vk::ImageLayout::ePreinitialized:
				// Image is preinitialized
				// Only valid as initial layout for linear images, preserves memory contents
				// Make sure host writes have been finished
				imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eHostWrite;
				break;

			case vk::ImageLayout::eColorAttachmentOptimal:
				// Image is a color attachment
				// Make sure any writes to the color buffer have been finished
				imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
				break;

			case vk::ImageLayout::eDepthStencilAttachmentOptimal:
				// Image is a depth/stencil attachment
				// Make sure any writes to the depth/stencil buffer have been finished
				imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
				break;

			case vk::ImageLayout::eTransferSrcOptimal:
				// Image is a transfer source
				// Make sure any reads from the image have been finished
				imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
				break;

			case vk::ImageLayout::eTransferDstOptimal:
				// Image is a transfer destination
				// Make sure any writes to the image have been finished
				imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
				break;

			case vk::ImageLayout::eShaderReadOnlyOptimal:
				// Image is read by a shader
				// Make sure any shader reads from the image have been finished
				imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
				break;
			default:
				// Other source layouts aren't handled (yet)
				break;
			}

			// Target layouts (new)
			// Destination access mask controls the dependency for the new image layout
			switch (newImageLayout)
			{
			case vk::ImageLayout::eTransferDstOptimal:
				// Image will be used as a transfer destination
				// Make sure any writes to the image have been finished
				imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
				break;

			case vk::ImageLayout::eTransferSrcOptimal:
				// Image will be used as a transfer source
				// Make sure any reads from the image have been finished
				imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
				break;

			case vk::ImageLayout::eColorAttachmentOptimal:
				// Image will be used as a color attachment
				// Make sure any writes to the color buffer have been finished
				imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
				break;

			case vk::ImageLayout::eDepthStencilAttachmentOptimal:
				// Image layout will be used as a depth/stencil attachment
				// Make sure any writes to depth/stencil buffer have been finished
				imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
				break;

			case vk::ImageLayout::eShaderReadOnlyOptimal:
				// Image will be read in a shader (sampler, input attachment)
				// Make sure any writes to the image have been finished
				if (!imageMemoryBarrier.srcAccessMask)
				{
					imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eHostWrite | vk::AccessFlagBits::eTransferWrite;
				}
				imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				break;
			default:
				// Other source layouts aren't handled (yet)
				break;
			}

			// Put barrier inside setup command buffer
			cmdbuffer.pipelineBarrier(srcStageMask, dstStageMask, {}, {}, {}, {imageMemoryBarrier});
		}

		// Fixed sub resource on first mip level and layer
		void setImageLayout(
			vk::CommandBuffer cmdbuffer,
			vk::Image image,
			vk::ImageAspectFlags aspectMask,
			vk::ImageLayout oldImageLayout,
			vk::ImageLayout newImageLayout,
			vk::PipelineStageFlags srcStageMask,
			vk::PipelineStageFlags dstStageMask)
		{
			vk::ImageSubresourceRange subresourceRange = {};
			subresourceRange.aspectMask = aspectMask;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = 1;
			subresourceRange.layerCount = 1;
			setImageLayout(cmdbuffer, image, oldImageLayout, newImageLayout, subresourceRange, srcStageMask, dstStageMask);
		}

		void insertImageMemoryBarrier(
			vk::CommandBuffer cmdbuffer,
			vk::Image image,
			vk::AccessFlags srcAccessMask,
			vk::AccessFlags dstAccessMask,
			vk::ImageLayout oldImageLayout,
			vk::ImageLayout newImageLayout,
			vk::PipelineStageFlags srcStageMask,
			vk::PipelineStageFlags dstStageMask,
			vk::ImageSubresourceRange subresourceRange)
		{
			vk::ImageMemoryBarrier imageMemoryBarrier = vks::initializers::imageMemoryBarrier();
			imageMemoryBarrier.srcAccessMask = srcAccessMask;
			imageMemoryBarrier.dstAccessMask = dstAccessMask;
			imageMemoryBarrier.oldLayout = oldImageLayout;
			imageMemoryBarrier.newLayout = newImageLayout;
			imageMemoryBarrier.image = image;
			imageMemoryBarrier.subresourceRange = subresourceRange;

			cmdbuffer.pipelineBarrier(srcStageMask, dstStageMask, {}, {}, {}, {imageMemoryBarrier});
		}

		void exitFatal(const std::string& message, int32_t exitCode)
		{
#if defined(_WIN32)
			if (!errorModeSilent) {
				MessageBox(NULL, message.c_str(), NULL, MB_OK | MB_ICONERROR);
			}
#endif
			std::cerr << message << "\n";
			exit(exitCode);
		}

		void exitFatal(const std::string& message, vk::Result resultCode)
		{
			exitFatal(message, (int32_t)resultCode);
		}

		vk::UniqueShaderModule loadShader(const char *fileName, vk::Device device)
		{
			std::ifstream is(fileName, std::ios::binary | std::ios::in | std::ios::ate);

			if (is.is_open())
			{
				size_t size = is.tellg();
				is.seekg(0, std::ios::beg);
				auto shaderCode = std::make_unique<char[]>(size);
				is.read(shaderCode.get(), size);
				is.close();

				assert(size > 0);

				vk::UniqueShaderModule shaderModule;
				vk::ShaderModuleCreateInfo moduleCreateInfo{};
				moduleCreateInfo.codeSize = size;
				moduleCreateInfo.pCode = (uint32_t*)shaderCode.get();

				shaderModule = device.createShaderModuleUnique(moduleCreateInfo);

				shaderCode.reset();

				return shaderModule;
			}
			else
			{
				std::cerr << "Error: Could not open shader file \"" << fileName << "\"" << "\n";
				return {};
			}
		}

		bool fileExists(const std::string &filename)
		{
			std::ifstream f(filename.c_str());
			return !f.fail();
		}
	}
}