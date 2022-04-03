/*
* Initializers for Vulkan structures and objects used by the examples
* Saves lot of VK_STRUCTURE_TYPE assignments
* Some initializers are parameterized for convenience
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <vector>
#include "vulkan/vulkan.h"

namespace vks
{
	namespace initializers
	{

		inline vk::MemoryAllocateInfo memoryAllocateInfo()
		{
			vk::MemoryAllocateInfo memAllocInfo {};
			return memAllocInfo;
		}

		inline vk::MappedMemoryRange mappedMemoryRange()
		{
			vk::MappedMemoryRange mappedMemoryRange {};
			return mappedMemoryRange;
		}

		inline vk::CommandBufferAllocateInfo commandBufferAllocateInfo(
			vk::CommandPool commandPool,
			vk::CommandBufferLevel level,
			uint32_t bufferCount)
		{
			vk::CommandBufferAllocateInfo commandBufferAllocateInfo {};
			commandBufferAllocateInfo.commandPool = commandPool;
			commandBufferAllocateInfo.level = level;
			commandBufferAllocateInfo.commandBufferCount = bufferCount;
			return commandBufferAllocateInfo;
		}

		inline vk::CommandPoolCreateInfo commandPoolCreateInfo()
		{
			vk::CommandPoolCreateInfo cmdPoolCreateInfo {};
			return cmdPoolCreateInfo;
		}

		inline vk::CommandBufferBeginInfo commandBufferBeginInfo()
		{
			vk::CommandBufferBeginInfo cmdBufferBeginInfo {};
			return cmdBufferBeginInfo;
		}

		inline vk::CommandBufferInheritanceInfo commandBufferInheritanceInfo()
		{
			vk::CommandBufferInheritanceInfo cmdBufferInheritanceInfo {};
			return cmdBufferInheritanceInfo;
		}

		inline vk::RenderPassBeginInfo renderPassBeginInfo()
		{
			vk::RenderPassBeginInfo renderPassBeginInfo {};
			return renderPassBeginInfo;
		}

		inline vk::RenderPassCreateInfo2 renderPassCreateInfo()
		{
			vk::RenderPassCreateInfo2 renderPassCreateInfo {};
			return renderPassCreateInfo;
		}

		/** @brief Initialize an image memory barrier with no image transfer ownership */
		inline vk::ImageMemoryBarrier imageMemoryBarrier()
		{
			vk::ImageMemoryBarrier imageMemoryBarrier {};
			imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			return imageMemoryBarrier;
		}

		/** @brief Initialize a buffer memory barrier with no image transfer ownership */
		inline vk::BufferMemoryBarrier bufferMemoryBarrier()
		{
			vk::BufferMemoryBarrier bufferMemoryBarrier {};
			bufferMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			return bufferMemoryBarrier;
		}

		inline vk::MemoryBarrier memoryBarrier()
		{
			vk::MemoryBarrier memoryBarrier {};
			return memoryBarrier;
		}

		inline vk::ImageCreateInfo imageCreateInfo()
		{
			vk::ImageCreateInfo imageCreateInfo {};
			return imageCreateInfo;
		}

		inline vk::SamplerCreateInfo samplerCreateInfo()
		{
			vk::SamplerCreateInfo samplerCreateInfo {};
			samplerCreateInfo.maxAnisotropy = 1.0f;
			return samplerCreateInfo;
		}

		inline vk::ImageViewCreateInfo imageViewCreateInfo()
		{
			vk::ImageViewCreateInfo imageViewCreateInfo {};
			return imageViewCreateInfo;
		}

		inline vk::FramebufferCreateInfo framebufferCreateInfo()
		{
			vk::FramebufferCreateInfo framebufferCreateInfo {};
			return framebufferCreateInfo;
		}

		inline vk::SemaphoreCreateInfo semaphoreCreateInfo()
		{
			vk::SemaphoreCreateInfo semaphoreCreateInfo {};
			return semaphoreCreateInfo;
		}

		inline vk::FenceCreateInfo fenceCreateInfo(vk::FenceCreateFlags flags = {})
		{
			vk::FenceCreateInfo fenceCreateInfo {};
			fenceCreateInfo.flags = flags;
			return fenceCreateInfo;
		}

		inline vk::EventCreateInfo eventCreateInfo()
		{
			vk::EventCreateInfo eventCreateInfo {};
			return eventCreateInfo;
		}

		inline vk::SubmitInfo submitInfo()
		{
			vk::SubmitInfo submitInfo {};
			return submitInfo;
		}

		inline vk::Viewport viewport(
			float width,
			float height,
			float minDepth,
			float maxDepth)
		{
			vk::Viewport viewport {};
			viewport.width = width;
			viewport.height = height;
			viewport.minDepth = minDepth;
			viewport.maxDepth = maxDepth;
			return viewport;
		}

		inline vk::Rect2D rect2D(
			int32_t width,
			int32_t height,
			int32_t offsetX,
			int32_t offsetY)
		{
			vk::Rect2D rect2D {};
			rect2D.extent.width = width;
			rect2D.extent.height = height;
			rect2D.offset.x = offsetX;
			rect2D.offset.y = offsetY;
			return rect2D;
		}

		inline vk::BufferCreateInfo bufferCreateInfo()
		{
			vk::BufferCreateInfo bufCreateInfo {};
			return bufCreateInfo;
		}

		inline vk::BufferCreateInfo bufferCreateInfo(
			vk::BufferUsageFlags usage,
			vk::DeviceSize size)
		{
			vk::BufferCreateInfo bufCreateInfo {};
			bufCreateInfo.usage = usage;
			bufCreateInfo.size = size;
			return bufCreateInfo;
		}

		inline vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo(
			uint32_t poolSizeCount,
			vk::DescriptorPoolSize* pPoolSizes,
			uint32_t maxSets)
		{
			vk::DescriptorPoolCreateInfo descriptorPoolInfo {};
			descriptorPoolInfo.poolSizeCount = poolSizeCount;
			descriptorPoolInfo.pPoolSizes = pPoolSizes;
			descriptorPoolInfo.maxSets = maxSets;
			return descriptorPoolInfo;
		}

		inline vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo(
			const std::vector<vk::DescriptorPoolSize>& poolSizes,
			uint32_t maxSets)
		{
			vk::DescriptorPoolCreateInfo descriptorPoolInfo{};
			descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
			descriptorPoolInfo.pPoolSizes = poolSizes.data();
			descriptorPoolInfo.maxSets = maxSets;
			return descriptorPoolInfo;
		}

		inline vk::DescriptorPoolSize descriptorPoolSize(
			vk::DescriptorType type,
			uint32_t descriptorCount)
		{
			vk::DescriptorPoolSize descriptorPoolSize {};
			descriptorPoolSize.type = type;
			descriptorPoolSize.descriptorCount = descriptorCount;
			return descriptorPoolSize;
		}

		inline vk::DescriptorSetLayoutBinding descriptorSetLayoutBinding(
			vk::DescriptorType type,
			vk::ShaderStageFlags stageFlags,
			uint32_t binding,
			uint32_t descriptorCount = 1)
		{
			vk::DescriptorSetLayoutBinding setLayoutBinding {};
			setLayoutBinding.descriptorType = type;
			setLayoutBinding.stageFlags = stageFlags;
			setLayoutBinding.binding = binding;
			setLayoutBinding.descriptorCount = descriptorCount;
			return setLayoutBinding;
		}

		inline vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(
			const vk::DescriptorSetLayoutBinding* pBindings,
			uint32_t bindingCount)
		{
			vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo {};
			descriptorSetLayoutCreateInfo.pBindings = pBindings;
			descriptorSetLayoutCreateInfo.bindingCount = bindingCount;
			return descriptorSetLayoutCreateInfo;
		}

		inline vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(
			const std::vector<vk::DescriptorSetLayoutBinding>& bindings)
		{
			vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
			descriptorSetLayoutCreateInfo.pBindings = bindings.data();
			descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
			return descriptorSetLayoutCreateInfo;
		}

		inline vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo(
			const vk::DescriptorSetLayout* pSetLayouts,
			uint32_t setLayoutCount = 1)
		{
			vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo {};
			pipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
			pipelineLayoutCreateInfo.pSetLayouts = pSetLayouts;
			return pipelineLayoutCreateInfo;
		}

		inline vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo(
			uint32_t setLayoutCount = 1)
		{
			vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
			pipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
			return pipelineLayoutCreateInfo;
		}

		inline vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo(
			vk::DescriptorPool descriptorPool,
			const vk::DescriptorSetLayout* pSetLayouts,
			uint32_t descriptorSetCount)
		{
			vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo {};
			descriptorSetAllocateInfo.descriptorPool = descriptorPool;
			descriptorSetAllocateInfo.pSetLayouts = pSetLayouts;
			descriptorSetAllocateInfo.descriptorSetCount = descriptorSetCount;
			return descriptorSetAllocateInfo;
		}

		inline vk::DescriptorImageInfo descriptorImageInfo(vk::Sampler sampler, vk::ImageView imageView, vk::ImageLayout imageLayout)
		{
			vk::DescriptorImageInfo descriptorImageInfo {};
			descriptorImageInfo.sampler = sampler;
			descriptorImageInfo.imageView = imageView;
			descriptorImageInfo.imageLayout = imageLayout;
			return descriptorImageInfo;
		}

		inline vk::WriteDescriptorSet writeDescriptorSet(
			vk::DescriptorSet dstSet,
			vk::DescriptorType type,
			uint32_t binding,
			vk::DescriptorBufferInfo* bufferInfo,
			uint32_t descriptorCount = 1)
		{
			vk::WriteDescriptorSet writeDescriptorSet {};
			writeDescriptorSet.dstSet = dstSet;
			writeDescriptorSet.descriptorType = type;
			writeDescriptorSet.dstBinding = binding;
			writeDescriptorSet.pBufferInfo = bufferInfo;
			writeDescriptorSet.descriptorCount = descriptorCount;
			return writeDescriptorSet;
		}

		inline vk::WriteDescriptorSet writeDescriptorSet(
			vk::DescriptorSet dstSet,
			vk::DescriptorType type,
			uint32_t binding,
			vk::DescriptorImageInfo *imageInfo,
			uint32_t descriptorCount = 1)
		{
			vk::WriteDescriptorSet writeDescriptorSet {};
			writeDescriptorSet.dstSet = dstSet;
			writeDescriptorSet.descriptorType = type;
			writeDescriptorSet.dstBinding = binding;
			writeDescriptorSet.pImageInfo = imageInfo;
			writeDescriptorSet.descriptorCount = descriptorCount;
			return writeDescriptorSet;
		}

		inline vk::VertexInputBindingDescription vertexInputBindingDescription(
			uint32_t binding,
			uint32_t stride,
			vk::VertexInputRate inputRate)
		{
			vk::VertexInputBindingDescription vInputBindDescription {};
			vInputBindDescription.binding = binding;
			vInputBindDescription.stride = stride;
			vInputBindDescription.inputRate = inputRate;
			return vInputBindDescription;
		}

		inline vk::VertexInputAttributeDescription vertexInputAttributeDescription(
			uint32_t binding,
			uint32_t location,
			vk::Format format,
			uint32_t offset)
		{
			vk::VertexInputAttributeDescription vInputAttribDescription {};
			vInputAttribDescription.location = location;
			vInputAttribDescription.binding = binding;
			vInputAttribDescription.format = format;
			vInputAttribDescription.offset = offset;
			return vInputAttribDescription;
		}

		inline vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo()
		{
			vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo {};
			return pipelineVertexInputStateCreateInfo;
		}

		inline vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo(
			const std::vector<vk::VertexInputBindingDescription> &vertexBindingDescriptions,
			const std::vector<vk::VertexInputAttributeDescription> &vertexAttributeDescriptions
		)
		{
			vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{};
			pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexBindingDescriptions.size());
			pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = vertexBindingDescriptions.data();
			pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescriptions.size());
			pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions.data();
			return pipelineVertexInputStateCreateInfo;
		}

		inline vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo(
			vk::PrimitiveTopology topology,
			vk::PipelineInputAssemblyStateCreateFlags flags,
			vk::Bool32 primitiveRestartEnable)
		{
			vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo {};
			pipelineInputAssemblyStateCreateInfo.topology = topology;
			pipelineInputAssemblyStateCreateInfo.flags = flags;
			pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = primitiveRestartEnable;
			return pipelineInputAssemblyStateCreateInfo;
		}

		inline vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo(
			vk::PolygonMode polygonMode,
			vk::CullModeFlags cullMode,
			vk::FrontFace frontFace,
			vk::PipelineRasterizationStateCreateFlags flags = {})
		{
			vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo {};
			pipelineRasterizationStateCreateInfo.polygonMode = polygonMode;
			pipelineRasterizationStateCreateInfo.cullMode = cullMode;
			pipelineRasterizationStateCreateInfo.frontFace = frontFace;
			pipelineRasterizationStateCreateInfo.flags = flags;
			pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
			pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;
			return pipelineRasterizationStateCreateInfo;
		}

		inline vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentState(
			vk::ColorComponentFlags colorWriteMask,
			vk::Bool32 blendEnable)
		{
			vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentState {};
			pipelineColorBlendAttachmentState.colorWriteMask = colorWriteMask;
			pipelineColorBlendAttachmentState.blendEnable = blendEnable;
			return pipelineColorBlendAttachmentState;
		}

		inline vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo(
			uint32_t attachmentCount,
			const vk::PipelineColorBlendAttachmentState * pAttachments)
		{
			vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo {};
			pipelineColorBlendStateCreateInfo.attachmentCount = attachmentCount;
			pipelineColorBlendStateCreateInfo.pAttachments = pAttachments;
			return pipelineColorBlendStateCreateInfo;
		}

		inline vk::PipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo(
			vk::Bool32 depthTestEnable,
			vk::Bool32 depthWriteEnable,
			vk::CompareOp depthCompareOp)
		{
			vk::PipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo {};
			pipelineDepthStencilStateCreateInfo.depthTestEnable = depthTestEnable;
			pipelineDepthStencilStateCreateInfo.depthWriteEnable = depthWriteEnable;
			pipelineDepthStencilStateCreateInfo.depthCompareOp = depthCompareOp;
			pipelineDepthStencilStateCreateInfo.back.compareOp = vk::CompareOp::eAlways;
			return pipelineDepthStencilStateCreateInfo;
		}

		inline vk::PipelineViewportStateCreateInfo pipelineViewportStateCreateInfo(
			uint32_t viewportCount,
			uint32_t scissorCount,
			vk::PipelineViewportStateCreateFlags flags = {})
		{
			vk::PipelineViewportStateCreateInfo pipelineViewportStateCreateInfo {};
			pipelineViewportStateCreateInfo.viewportCount = viewportCount;
			pipelineViewportStateCreateInfo.scissorCount = scissorCount;
			pipelineViewportStateCreateInfo.flags = flags;
			return pipelineViewportStateCreateInfo;
		}

		inline vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo(
			vk::SampleCountFlagBits rasterizationSamples,
			vk::PipelineMultisampleStateCreateFlags flags = {})
		{
			vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo {};
			pipelineMultisampleStateCreateInfo.rasterizationSamples = rasterizationSamples;
			pipelineMultisampleStateCreateInfo.flags = flags;
			return pipelineMultisampleStateCreateInfo;
		}

		inline vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(
			const vk::DynamicState * pDynamicStates,
			uint32_t dynamicStateCount,
			vk::PipelineDynamicStateCreateFlags flags = {})
		{
			vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo {};
			pipelineDynamicStateCreateInfo.pDynamicStates = pDynamicStates;
			pipelineDynamicStateCreateInfo.dynamicStateCount = dynamicStateCount;
			pipelineDynamicStateCreateInfo.flags = flags;
			return pipelineDynamicStateCreateInfo;
		}

		inline vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(
			const std::vector<vk::DynamicState>& pDynamicStates,
			vk::PipelineDynamicStateCreateFlags flags = {})
		{
			vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
			pipelineDynamicStateCreateInfo.pDynamicStates = pDynamicStates.data();
			pipelineDynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(pDynamicStates.size());
			pipelineDynamicStateCreateInfo.flags = flags;
			return pipelineDynamicStateCreateInfo;
		}

		inline vk::PipelineTessellationStateCreateInfo pipelineTessellationStateCreateInfo(uint32_t patchControlPoints)
		{
			vk::PipelineTessellationStateCreateInfo pipelineTessellationStateCreateInfo {};
			pipelineTessellationStateCreateInfo.patchControlPoints = patchControlPoints;
			return pipelineTessellationStateCreateInfo;
		}

		inline vk::GraphicsPipelineCreateInfo pipelineCreateInfo(
			vk::PipelineLayout layout,
			vk::RenderPass renderPass,
			vk::PipelineCreateFlags flags = {})
		{
			vk::GraphicsPipelineCreateInfo pipelineCreateInfo {};
			pipelineCreateInfo.layout = layout;
			pipelineCreateInfo.renderPass = renderPass;
			pipelineCreateInfo.flags = flags;
			pipelineCreateInfo.basePipelineIndex = -1;
			return pipelineCreateInfo;
		}

		inline vk::GraphicsPipelineCreateInfo pipelineCreateInfo()
		{
			vk::GraphicsPipelineCreateInfo pipelineCreateInfo{};
			pipelineCreateInfo.basePipelineIndex = -1;
			return pipelineCreateInfo;
		}

		inline vk::ComputePipelineCreateInfo computePipelineCreateInfo(
			vk::PipelineLayout layout,
			vk::PipelineCreateFlags flags = {})
		{
			vk::ComputePipelineCreateInfo computePipelineCreateInfo {};
			computePipelineCreateInfo.layout = layout;
			computePipelineCreateInfo.flags = flags;
			return computePipelineCreateInfo;
		}

		inline vk::PushConstantRange pushConstantRange(
			vk::ShaderStageFlags stageFlags,
			uint32_t size,
			uint32_t offset)
		{
			vk::PushConstantRange pushConstantRange {};
			pushConstantRange.stageFlags = stageFlags;
			pushConstantRange.offset = offset;
			pushConstantRange.size = size;
			return pushConstantRange;
		}

		inline vk::BindSparseInfo bindSparseInfo()
		{
			vk::BindSparseInfo bindSparseInfo{};
			return bindSparseInfo;
		}

		/** @brief Initialize a map entry for a shader specialization constant */
		inline vk::SpecializationMapEntry specializationMapEntry(uint32_t constantID, uint32_t offset, size_t size)
		{
			vk::SpecializationMapEntry specializationMapEntry{};
			specializationMapEntry.constantID = constantID;
			specializationMapEntry.offset = offset;
			specializationMapEntry.size = size;
			return specializationMapEntry;
		}

		/** @brief Initialize a specialization constant info structure to pass to a shader stage */
		inline vk::SpecializationInfo specializationInfo(uint32_t mapEntryCount, const vk::SpecializationMapEntry* mapEntries, size_t dataSize, const void* data)
		{
			vk::SpecializationInfo specializationInfo{};
			specializationInfo.mapEntryCount = mapEntryCount;
			specializationInfo.pMapEntries = mapEntries;
			specializationInfo.dataSize = dataSize;
			specializationInfo.pData = data;
			return specializationInfo;
		}

		/** @brief Initialize a specialization constant info structure to pass to a shader stage */
		inline vk::SpecializationInfo specializationInfo(const std::vector<vk::SpecializationMapEntry> &mapEntries, size_t dataSize, const void* data)
		{
			vk::SpecializationInfo specializationInfo{};
			specializationInfo.mapEntryCount = static_cast<uint32_t>(mapEntries.size());
			specializationInfo.pMapEntries = mapEntries.data();
			specializationInfo.dataSize = dataSize;
			specializationInfo.pData = data;
			return specializationInfo;
		}

	}
}