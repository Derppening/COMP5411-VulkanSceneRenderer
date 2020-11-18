/*
* UI overlay class using ImGui
*
* Copyright (C) 2017 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <sstream>
#include <iomanip>

#include <vulkan/vulkan.h>
#include "VulkanTools.h"
#include "VulkanDebug.h"
#include "VulkanBuffer.h"
#include "VulkanDevice.h"

#include <imgui.h>

namespace vks 
{
	class UIOverlay 
	{
	public:
		vks::VulkanDevice *device;
		vk::Queue queue;

		vk::SampleCountFlagBits rasterizationSamples = vk::SampleCountFlagBits::e1;
		uint32_t subpass = 0;

		vks::Buffer vertexBuffer;
		vks::Buffer indexBuffer;
		int32_t vertexCount = 0;
		int32_t indexCount = 0;

		std::vector<vk::PipelineShaderStageCreateInfo> shaders;

		vk::DescriptorPool descriptorPool;
		vk::DescriptorSetLayout descriptorSetLayout;
		vk::DescriptorSet descriptorSet;
		vk::PipelineLayout pipelineLayout;
		vk::Pipeline pipeline;

		vk::DeviceMemory fontMemory;
		vk::Image fontImage;
		vk::ImageView fontView;
		vk::Sampler sampler;

		struct PushConstBlock {
			glm::vec2 scale;
			glm::vec2 translate;
		} pushConstBlock;

		bool visible = true;
		bool updated = false;
		float scale = 1.0f;

		UIOverlay();
		~UIOverlay();

		void preparePipeline(const vk::PipelineCache pipelineCache, const vk::RenderPass renderPass);
		void prepareResources();

		bool update();
		void draw(const vk::CommandBuffer commandBuffer);
		void resize(uint32_t width, uint32_t height);

		void freeResources();

		bool header(const char* caption);
		bool checkBox(const char* caption, bool* value);
		bool checkBox(const char* caption, int32_t* value);
		bool inputFloat(const char* caption, float* value, float step, uint32_t precision);
		bool sliderFloat(const char* caption, float* value, float min, float max);
		bool sliderInt(const char* caption, int32_t* value, int32_t min, int32_t max);
		bool comboBox(const char* caption, int32_t* itemindex, std::vector<std::string> items);
		bool button(const char* caption);
		void text(const char* formatstr, ...);
	};
}