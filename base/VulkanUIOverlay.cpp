
/*
* UI overlay class using ImGui
*
* Copyright (C) 2017 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "VulkanUIOverlay.h"

#include <fmt/format.h>

namespace vks 
{
	UIOverlay::UIOverlay()
	{

		// Init ImGui
		ImGui::CreateContext();
		// Color scheme
		ImGuiStyle& style = ImGui::GetStyle();
		style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.0f, 0.0f, 0.0f, 0.1f);
		style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
		style.Colors[ImGuiCol_Header] = ImVec4(0.8f, 0.0f, 0.0f, 0.4f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
		style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
		style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
		style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
		style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
		style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);
		style.Colors[ImGuiCol_Button] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
		// Dimensions
		ImGuiIO& io = ImGui::GetIO();
		io.FontGlobalScale = scale;
	}

	UIOverlay::~UIOverlay()	{
        if (ImGui::GetCurrentContext()) {
            ImGui::DestroyContext();
        }
    }

	/** Prepare all vulkan resources required to render the UI overlay */
	void UIOverlay::prepareResources()
	{
		ImGuiIO& io = ImGui::GetIO();

		// Create font texture
		unsigned char* fontData;
		int texWidth, texHeight;
		const std::string filename = getAssetPath() + "Roboto-Medium.ttf";
		io.Fonts->AddFontFromFileTTF(filename.c_str(), 16.0f);
		io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
		vk::DeviceSize uploadSize = texWidth*texHeight * 4 * sizeof(char);

		// Create target image for copy
		vk::ImageCreateInfo imageInfo = vks::initializers::imageCreateInfo();
		imageInfo.imageType = vk::ImageType::e2D;
		imageInfo.format = vk::Format::eR8G8B8A8Unorm;
		imageInfo.extent.width = texWidth;
		imageInfo.extent.height = texHeight;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = vk::SampleCountFlagBits::e1;
		imageInfo.tiling = vk::ImageTiling::eOptimal;
		imageInfo.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
		imageInfo.sharingMode = vk::SharingMode::eExclusive;
		imageInfo.initialLayout = vk::ImageLayout::eUndefined;
		fontImage = device->logicalDevice->createImageUnique(imageInfo);
		vk::MemoryRequirements2 memReqs = device->logicalDevice->getImageMemoryRequirements2(*fontImage);
		vk::MemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		memAllocInfo.allocationSize = memReqs.memoryRequirements.size;
		memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
		fontMemory = device->logicalDevice->allocateMemoryUnique(memAllocInfo);
		device->logicalDevice->bindImageMemory(*fontImage, *fontMemory, 0);

		// Image view
		vk::ImageViewCreateInfo viewInfo = vks::initializers::imageViewCreateInfo();
		viewInfo.image = *fontImage;
		viewInfo.viewType = vk::ImageViewType::e2D;
		viewInfo.format = vk::Format::eR8G8B8A8Unorm;
		viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.layerCount = 1;
		fontView = device->logicalDevice->createImageViewUnique(viewInfo);

		// Staging buffers for font data upload
		vks::Buffer stagingBuffer;

		VK_CHECK_RESULT(device->createBuffer(
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			&stagingBuffer,
			uploadSize));

		stagingBuffer.map();
		std::copy_n(reinterpret_cast<std::byte*>(fontData), uploadSize, static_cast<std::byte*>(stagingBuffer.mapped));
		stagingBuffer.unmap();

		// Copy buffer data to font image
		vk::UniqueCommandBuffer copyCmd = device->createCommandBuffer(vk::CommandBufferLevel::ePrimary, true);

		// Prepare for transfer
		vks::tools::setImageLayout(
			*copyCmd,
			*fontImage,
			vk::ImageAspectFlagBits::eColor,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal,
			vk::PipelineStageFlagBits::eHost,
			vk::PipelineStageFlagBits::eTransfer);

		// Copy
		vk::BufferImageCopy bufferCopyRegion = {};
		bufferCopyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		bufferCopyRegion.imageSubresource.layerCount = 1;
		bufferCopyRegion.imageExtent.width = texWidth;
		bufferCopyRegion.imageExtent.height = texHeight;
		bufferCopyRegion.imageExtent.depth = 1;

		copyCmd->copyBufferToImage(*stagingBuffer.buffer, *fontImage, vk::ImageLayout::eTransferDstOptimal, {bufferCopyRegion});

		// Prepare for shader read
		vks::tools::setImageLayout(
			*copyCmd,
			*fontImage,
			vk::ImageAspectFlagBits::eColor,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eFragmentShader);

		device->flushCommandBuffer(copyCmd, queue, true);

		stagingBuffer.destroy();

		// Font texture Sampler
		vk::SamplerCreateInfo samplerInfo = vks::initializers::samplerCreateInfo();
		samplerInfo.magFilter = vk::Filter::eLinear;
		samplerInfo.minFilter = vk::Filter::eLinear;
		samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
		samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite;
		sampler = device->logicalDevice->createSamplerUnique(samplerInfo);

		// Descriptor pool
		std::vector<vk::DescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1)
		};
		vk::DescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
		descriptorPool = device->logicalDevice->createDescriptorPoolUnique(descriptorPoolInfo);

		// Descriptor set layout
		std::vector<vk::DescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 0),
		};
		vk::DescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		descriptorSetLayout = device->logicalDevice->createDescriptorSetLayoutUnique(descriptorLayout);

		// Descriptor set
		vk::DescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(*descriptorPool, &*descriptorSetLayout, 1);
		descriptorSet = device->logicalDevice->allocateDescriptorSets(allocInfo)[0];
		vk::DescriptorImageInfo fontDescriptor = vks::initializers::descriptorImageInfo(
			*sampler,
			*fontView,
			vk::ImageLayout::eShaderReadOnlyOptimal
		);
		std::vector<vk::WriteDescriptorSet> writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSet, vk::DescriptorType::eCombinedImageSampler, 0, &fontDescriptor)
		};
		device->logicalDevice->updateDescriptorSets(writeDescriptorSets, {});
	}

	/** Prepare a separate pipeline for the UI overlay rendering decoupled from the main application */
	void UIOverlay::preparePipeline(const vk::PipelineCache pipelineCache, const vk::RenderPass renderPass)
	{
		// Pipeline layout
		// Push constants for UI rendering parameters
		vk::PushConstantRange pushConstantRange = vks::initializers::pushConstantRange(vk::ShaderStageFlagBits::eVertex, sizeof(PushConstBlock), 0);
		vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&*descriptorSetLayout, 1);
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
		pipelineLayout = device->logicalDevice->createPipelineLayoutUnique(pipelineLayoutCreateInfo);

		// Setup graphics pipeline for UI rendering
		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState =
			vks::initializers::pipelineInputAssemblyStateCreateInfo(vk::PrimitiveTopology::eTriangleList, {}, VK_FALSE);

		vk::PipelineRasterizationStateCreateInfo rasterizationState =
			vks::initializers::pipelineRasterizationStateCreateInfo(vk::PolygonMode::eFill, vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise);

		// Enable blending
		vk::PipelineColorBlendAttachmentState blendAttachmentState{};
		blendAttachmentState.blendEnable = VK_TRUE;
		blendAttachmentState.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eG;
		blendAttachmentState.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
		blendAttachmentState.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
		blendAttachmentState.colorBlendOp = vk::BlendOp::eAdd;
		blendAttachmentState.srcAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
		blendAttachmentState.dstAlphaBlendFactor = vk::BlendFactor::eZero;
		blendAttachmentState.alphaBlendOp = vk::BlendOp::eAdd;

		vk::PipelineColorBlendStateCreateInfo colorBlendState =
			vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

		vk::PipelineDepthStencilStateCreateInfo depthStencilState =
			vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, vk::CompareOp::eAlways);

		vk::PipelineViewportStateCreateInfo viewportState =
			vks::initializers::pipelineViewportStateCreateInfo(1, 1, {});

		vk::PipelineMultisampleStateCreateInfo multisampleState =
			vks::initializers::pipelineMultisampleStateCreateInfo(rasterizationSamples);

		std::vector<vk::DynamicState> dynamicStateEnables = {
		    vk::DynamicState::eViewport,
		    vk::DynamicState::eScissor
		};
		vk::PipelineDynamicStateCreateInfo dynamicState =
			vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

		vk::GraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo(*pipelineLayout, renderPass);

		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaders.size());
		pipelineCreateInfo.pStages = shaders.data();
		pipelineCreateInfo.subpass = subpass;

		// Vertex bindings an attributes based on ImGui vertex definition
		std::vector<vk::VertexInputBindingDescription> vertexInputBindings = {
			vks::initializers::vertexInputBindingDescription(0, sizeof(ImDrawVert), vk::VertexInputRate::eVertex),
		};
		std::vector<vk::VertexInputAttributeDescription> vertexInputAttributes = {
			vks::initializers::vertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(ImDrawVert, pos)),	// Location 0: Position
			vks::initializers::vertexInputAttributeDescription(0, 1, vk::Format::eR32G32Sfloat, offsetof(ImDrawVert, uv)),	// Location 1: UV
			vks::initializers::vertexInputAttributeDescription(0, 2, vk::Format::eR8G8B8A8Unorm, offsetof(ImDrawVert, col)),	// Location 0: Color
		};
		vk::PipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
		vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
		vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

		pipelineCreateInfo.pVertexInputState = &vertexInputState;

		pipeline = device->logicalDevice->createGraphicsPipelineUnique(pipelineCache, pipelineCreateInfo).value;
	}

	/** Update vertex and index buffer containing the imGui elements when required */
	bool UIOverlay::update()
	{
		ImDrawData* imDrawData = ImGui::GetDrawData();
		bool updateCmdBuffers = false;

		if (!imDrawData) { return false; };

		// Note: Alignment is done inside buffer creation
		vk::DeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
		vk::DeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

		// Update buffers only if vertex or index count has been changed compared to current buffer size
		if ((vertexBufferSize == 0) || (indexBufferSize == 0)) {
			return false;
		}

		// Vertex buffer
		if ((vertexBuffer.buffer) || (vertexCount != imDrawData->TotalVtxCount)) {
			vertexBuffer.unmap();
			vertexBuffer.destroy();
			VK_CHECK_RESULT(device->createBuffer(vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eHostVisible, &vertexBuffer, vertexBufferSize));
			vertexCount = imDrawData->TotalVtxCount;
			vertexBuffer.unmap();
			vertexBuffer.map();
			updateCmdBuffers = true;
		}

		// Index buffer
		vk::DeviceSize indexSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);
		if ((indexBuffer.buffer) || (indexCount < imDrawData->TotalIdxCount)) {
			indexBuffer.unmap();
			indexBuffer.destroy();
			VK_CHECK_RESULT(device->createBuffer(vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eHostVisible, &indexBuffer, indexBufferSize));
			indexCount = imDrawData->TotalIdxCount;
			indexBuffer.map();
			updateCmdBuffers = true;
		}

		// Upload data
		ImDrawVert* vtxDst = (ImDrawVert*)vertexBuffer.mapped;
		ImDrawIdx* idxDst = (ImDrawIdx*)indexBuffer.mapped;

		for (int n = 0; n < imDrawData->CmdListsCount; n++) {
			const ImDrawList* cmd_list = imDrawData->CmdLists[n];
			std::copy_n(reinterpret_cast<std::byte*>(cmd_list->VtxBuffer.Data), cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), reinterpret_cast<std::byte*>(vtxDst));
			std::copy_n(reinterpret_cast<std::byte*>(cmd_list->IdxBuffer.Data), cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), reinterpret_cast<std::byte*>(idxDst));
			vtxDst += cmd_list->VtxBuffer.Size;
			idxDst += cmd_list->IdxBuffer.Size;
		}

		// Flush to make writes visible to GPU
		vertexBuffer.flush();
		indexBuffer.flush();

		return updateCmdBuffers;
	}

	void UIOverlay::draw(const vk::CommandBuffer commandBuffer)
	{
		ImDrawData* imDrawData = ImGui::GetDrawData();
		int32_t vertexOffset = 0;
		int32_t indexOffset = 0;

		if ((!imDrawData) || (imDrawData->CmdListsCount == 0)) {
			return;
		}

		ImGuiIO& io = ImGui::GetIO();

		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, {descriptorSet}, {});

		pushConstBlock.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
		pushConstBlock.translate = glm::vec2(-1.0f);
		commandBuffer.pushConstants<PushConstBlock>(*pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, {pushConstBlock});

		std::array<vk::DeviceSize, 1> offsets = { 0 };
		commandBuffer.bindVertexBuffers(0, {*vertexBuffer.buffer}, offsets);
		commandBuffer.bindIndexBuffer(*indexBuffer.buffer, 0, vk::IndexType::eUint16);

		for (int32_t i = 0; i < imDrawData->CmdListsCount; i++)
		{
			const ImDrawList* cmd_list = imDrawData->CmdLists[i];
			for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++)
			{
				const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
				vk::Rect2D scissorRect;
				scissorRect.offset.x = std::max((int32_t)(pcmd->ClipRect.x), 0);
				scissorRect.offset.y = std::max((int32_t)(pcmd->ClipRect.y), 0);
				scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
				scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
				commandBuffer.setScissor(0, {scissorRect});
				commandBuffer.drawIndexed(pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
				indexOffset += pcmd->ElemCount;
			}
			vertexOffset += cmd_list->VtxBuffer.Size;
		}
	}

	void UIOverlay::resize(uint32_t width, uint32_t height)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = ImVec2((float)(width), (float)(height));
	}

	void UIOverlay::freeResources()
	{
		ImGui::DestroyContext();
		vertexBuffer.destroy();
		indexBuffer.destroy();
		fontView.reset();
		fontImage.reset();
		fontMemory.reset();
		sampler.reset();
		descriptorSetLayout.reset();
		descriptorPool.reset();
		pipelineLayout.reset();
		pipeline.reset();
	}

	bool UIOverlay::header(const char *caption)
	{
		return ImGui::CollapsingHeader(caption, ImGuiTreeNodeFlags_DefaultOpen);
	}

	bool UIOverlay::checkBox(const char *caption, bool *value)
	{
		bool res = ImGui::Checkbox(caption, value);
		if (res) { updated = true; };
		return res;
	}

	bool UIOverlay::checkBox(const char *caption, int32_t *value)
	{
		bool val = (*value == 1);
		bool res = ImGui::Checkbox(caption, &val);
		*value = val;
		if (res) { updated = true; };
		return res;
	}

    bool UIOverlay::radioButton(const char* caption, bool value)
    {
        bool res = ImGui::RadioButton(caption, value);
        if (res) { updated = true; };
        return res;
    }

    bool UIOverlay::inputFloat(const char *caption, float *value, float step, uint32_t precision)
	{
        const std::string format_str = fmt::format("%.{}f", precision);
		bool res = ImGui::InputFloat(caption, value, step, step * 10.0f, format_str.c_str());
		if (res) { updated = true; };
		return res;
	}

	bool UIOverlay::sliderFloat(const char* caption, float* value, float min, float max)
	{
		bool res = ImGui::SliderFloat(caption, value, min, max);
		if (res) { updated = true; };
		return res;
	}

	bool UIOverlay::sliderInt(const char* caption, int32_t* value, int32_t min, int32_t max)
	{
		bool res = ImGui::SliderInt(caption, value, min, max);
		if (res) { updated = true; };
		return res;
	}

	bool UIOverlay::comboBox(const char *caption, int32_t *itemindex, std::vector<std::string> items)
	{
		if (items.empty()) {
			return false;
		}
		std::vector<const char*> charitems;
		charitems.reserve(items.size());
		for (size_t i = 0; i < items.size(); i++) {
			charitems.push_back(items[i].c_str());
		}
		uint32_t itemCount = static_cast<uint32_t>(charitems.size());
		bool res = ImGui::Combo(caption, itemindex, &charitems[0], itemCount, itemCount);
		if (res) { updated = true; };
		return res;
	}

	bool UIOverlay::button(const char *caption)
	{
		bool res = ImGui::Button(caption);
		if (res) { updated = true; };
		return res;
	}

	void UIOverlay::text(const char *formatstr, ...)
	{
		va_list args;
		va_start(args, formatstr);
		ImGui::TextV(formatstr, args);
		va_end(args);
	}
}