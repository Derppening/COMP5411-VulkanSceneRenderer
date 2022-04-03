/*
* Vulkan framebuffer class
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <algorithm>
#include <iterator>
#include <vector>
#include "vulkan/vulkan.h"
#include "VulkanDevice.h"
#include "VulkanTools.h"

namespace vks
{
	/**
	* @brief Encapsulates a single frame buffer attachment 
	*/
	struct FramebufferAttachment
	{
		vk::UniqueImage image;
		vk::UniqueDeviceMemory memory;
		vk::UniqueImageView view;
		vk::Format format;
		vk::ImageSubresourceRange subresourceRange;
		vk::AttachmentDescription2 description;

		/**
		* @brief Returns true if the attachment has a depth component
		*/
		bool hasDepth()
		{
			std::vector<vk::Format> formats = 
			{
				vk::Format::eD16Unorm,
				vk::Format::eX8D24UnormPack32,
				vk::Format::eD32Sfloat,
				vk::Format::eD16UnormS8Uint,
				vk::Format::eD24UnormS8Uint,
				vk::Format::eD32SfloatS8Uint,
			};
			return std::find(formats.begin(), formats.end(), format) != std::end(formats);
		}

		/**
		* @brief Returns true if the attachment has a stencil component
		*/
		bool hasStencil()
		{
			std::vector<vk::Format> formats = 
			{
				vk::Format::eS8Uint,
				vk::Format::eD16UnormS8Uint,
				vk::Format::eD24UnormS8Uint,
				vk::Format::eD32SfloatS8Uint,
			};
			return std::find(formats.begin(), formats.end(), format) != std::end(formats);
		}

		/**
		* @brief Returns true if the attachment is a depth and/or stencil attachment
		*/
		bool isDepthStencil()
		{
			return(hasDepth() || hasStencil());
		}

	};

	/**
	* @brief Describes the attributes of an attachment to be created
	*/
	struct AttachmentCreateInfo
	{
		uint32_t width, height;
		uint32_t layerCount;
		vk::Format format;
		vk::ImageUsageFlags usage;
		vk::SampleCountFlagBits imageSampleCount = vk::SampleCountFlagBits::e1;
	};

	/**
	* @brief Encapsulates a complete Vulkan framebuffer with an arbitrary number and combination of attachments
	*/
	struct Framebuffer
	{
	private:
		vks::VulkanDevice *vulkanDevice;
	public:
		uint32_t width, height;
		vk::Framebuffer framebuffer;
		vk::RenderPass renderPass;
		vk::Sampler sampler;
		std::vector<vks::FramebufferAttachment> attachments;

		/**
		* Default constructor
		*
		* @param vulkanDevice Pointer to a valid VulkanDevice
		*/
		Framebuffer(vks::VulkanDevice *vulkanDevice)
		{
			assert(vulkanDevice);
			this->vulkanDevice = vulkanDevice;
		}

		/**
		* Destroy and free Vulkan resources used for the framebuffer and all of its attachments
		*/
		~Framebuffer()
		{
			assert(vulkanDevice);
			for (auto& attachment : attachments)
			{
				attachment.image.reset();
				attachment.view.reset();
				attachment.memory.reset();
			}
			vulkanDevice->logicalDevice->destroy(sampler);
			vulkanDevice->logicalDevice->destroy(renderPass);
			vulkanDevice->logicalDevice->destroy(framebuffer);
		}

		/**
		* Add a new attachment described by createinfo to the framebuffer's attachment list
		*
		* @param createinfo Structure that specifies the framebuffer to be constructed
		*
		* @return Index of the new attachment
		*/
		uint32_t addAttachment(vks::AttachmentCreateInfo createinfo)
		{
			vks::FramebufferAttachment attachment;

			attachment.format = createinfo.format;

			vk::ImageAspectFlags aspectMask = {};

			// Select aspect mask and layout depending on usage

			// Color attachment
			if (createinfo.usage & vk::ImageUsageFlagBits::eColorAttachment)
			{
				aspectMask = vk::ImageAspectFlagBits::eColor;
			}

			// Depth (and/or stencil) attachment
			if (createinfo.usage & vk::ImageUsageFlagBits::eDepthStencilAttachment)
			{
				if (attachment.hasDepth())
				{
					aspectMask = vk::ImageAspectFlagBits::eDepth;
				}
				if (attachment.hasStencil())
				{
					aspectMask = aspectMask | vk::ImageAspectFlagBits::eStencil;
				}
			}

			assert(!aspectMask);

			vk::ImageCreateInfo image = vks::initializers::imageCreateInfo();
			image.imageType = vk::ImageType::e2D;
			image.format = createinfo.format;
			image.extent.width = createinfo.width;
			image.extent.height = createinfo.height;
			image.extent.depth = 1;
			image.mipLevels = 1;
			image.arrayLayers = createinfo.layerCount;
			image.samples = createinfo.imageSampleCount;
			image.tiling = vk::ImageTiling::eOptimal;
			image.usage = createinfo.usage;

			vk::MemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
			vk::MemoryRequirements2 memReqs;

			// Create image for this attachment
			attachment.image = vulkanDevice->logicalDevice->createImageUnique(image);
			memReqs = vulkanDevice->logicalDevice->getImageMemoryRequirements2(*attachment.image);
			memAlloc.allocationSize = memReqs.memoryRequirements.size;
			memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
			attachment.memory = vulkanDevice->logicalDevice->allocateMemoryUnique(memAlloc);
			vulkanDevice->logicalDevice->bindImageMemory(*attachment.image, *attachment.memory, {});

			attachment.subresourceRange = vk::ImageSubresourceRange{};
			attachment.subresourceRange.aspectMask = aspectMask;
			attachment.subresourceRange.levelCount = 1;
			attachment.subresourceRange.layerCount = createinfo.layerCount;

			vk::ImageViewCreateInfo imageView = vks::initializers::imageViewCreateInfo();
			imageView.viewType = (createinfo.layerCount == 1) ? vk::ImageViewType::e2D : vk::ImageViewType::e2DArray;
			imageView.format = createinfo.format;
			imageView.subresourceRange = attachment.subresourceRange;
			//todo: workaround for depth+stencil attachments
			imageView.subresourceRange.aspectMask = (attachment.hasDepth()) ? vk::ImageAspectFlagBits::eDepth : aspectMask;
			imageView.image = *attachment.image;
			attachment.view = vulkanDevice->logicalDevice->createImageViewUnique(imageView);

			// Fill attachment description
			attachment.description = vk::AttachmentDescription2{};
			attachment.description.samples = createinfo.imageSampleCount;
			attachment.description.loadOp = vk::AttachmentLoadOp::eClear;
			attachment.description.storeOp = (createinfo.usage & vk::ImageUsageFlagBits::eSampled) ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare;
			attachment.description.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
			attachment.description.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
			attachment.description.format = createinfo.format;
			attachment.description.initialLayout = vk::ImageLayout::eUndefined;
			// Final layout
			// If not, final layout depends on attachment type
			if (attachment.hasDepth() || attachment.hasStencil())
			{
				attachment.description.finalLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
			}
			else
			{
				attachment.description.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			}

			attachments.push_back(std::move(attachment));

			return static_cast<uint32_t>(attachments.size() - 1);
		}

		/**
		* Creates a default sampler for sampling from any of the framebuffer attachments
		* Applications are free to create their own samplers for different use cases 
		*
		* @param magFilter Magnification filter for lookups
		* @param minFilter Minification filter for lookups
		* @param adressMode Addressing mode for the U,V and W coordinates
		*
		* @return vk::Result for the sampler creation
		*/
		vk::Result createSampler(vk::Filter magFilter, vk::Filter minFilter, vk::SamplerAddressMode adressMode)
		{
			vk::SamplerCreateInfo samplerInfo = vks::initializers::samplerCreateInfo();
			samplerInfo.magFilter = magFilter;
			samplerInfo.minFilter = minFilter;
			samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
			samplerInfo.addressModeU = adressMode;
			samplerInfo.addressModeV = adressMode;
			samplerInfo.addressModeW = adressMode;
			samplerInfo.mipLodBias = 0.0f;
			samplerInfo.maxAnisotropy = 1.0f;
			samplerInfo.minLod = 0.0f;
			samplerInfo.maxLod = 1.0f;
			samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite;
			sampler = vulkanDevice->logicalDevice->createSampler(samplerInfo);
			return vk::Result::eSuccess;
		}

		/**
		* Creates a default render pass setup with one sub pass
		*
		* @return VK_SUCCESS if all resources have been created successfully
		*/
		vk::Result createRenderPass()
		{
			std::vector<vk::AttachmentDescription2> attachmentDescriptions;
			for (auto& attachment : attachments)
			{
				attachmentDescriptions.push_back(attachment.description);
			};

			// Collect attachment references
			std::vector<vk::AttachmentReference2> colorReferences;
			vk::AttachmentReference2 depthReference = {};
			bool hasDepth = false; 
			bool hasColor = false;

			uint32_t attachmentIndex = 0;

			for (auto& attachment : attachments)
			{
				if (attachment.isDepthStencil())
				{
					// Only one depth attachment allowed
					assert(!hasDepth);
					depthReference.attachment = attachmentIndex;
					depthReference.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
					hasDepth = true;
				}
				else
				{
					colorReferences.push_back({ attachmentIndex, vk::ImageLayout::eColorAttachmentOptimal });
					hasColor = true;
				}
				attachmentIndex++;
			};

			// Default render pass setup uses only one subpass
			vk::SubpassDescription2 subpass = {};
			subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
			if (hasColor)
			{
				subpass.pColorAttachments = colorReferences.data();
				subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
			}
			if (hasDepth)
			{
				subpass.pDepthStencilAttachment = &depthReference;
			}

			// Use subpass dependencies for attachment layout transitions
			std::array<vk::SubpassDependency2, 2> dependencies;

			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
			dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
			dependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
			dependencies[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
			dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;

			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
			dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
			dependencies[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
			dependencies[1].dstAccessMask = vk::AccessFlagBits::eMemoryRead;;
			dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

			// Create render pass
			vk::RenderPassCreateInfo2 renderPassInfo = {};
			renderPassInfo.pAttachments = attachmentDescriptions.data();
			renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = 2;
			renderPassInfo.pDependencies = dependencies.data();
			renderPass = vulkanDevice->logicalDevice->createRenderPass2(renderPassInfo);

			std::vector<vk::ImageView> attachmentViews;
			for (auto& attachment : attachments)
			{
				attachmentViews.push_back(*attachment.view);
			}

			// Find. max number of layers across attachments
			uint32_t maxLayers = 0;
			for (auto& attachment : attachments)
			{
				if (attachment.subresourceRange.layerCount > maxLayers)
				{
					maxLayers = attachment.subresourceRange.layerCount;
				}
			}

			vk::FramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.pAttachments = attachmentViews.data();
			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
			framebufferInfo.width = width;
			framebufferInfo.height = height;
			framebufferInfo.layers = maxLayers;
			framebuffer = vulkanDevice->logicalDevice->createFramebuffer(framebufferInfo);

			return vk::Result::eSuccess;
		}
	};
}