/*
* Vulkan examples debug wrapper
* 
* Copyright (C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "VulkanDebug.h"
#include <iostream>
#include "VulkanTools.h"

namespace vks
{


	namespace debug
	{
		vk::UniqueHandle<vk::DebugUtilsMessengerEXT, vk::DispatchLoaderDynamic> debugUtilsMessenger;

		VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessengerCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageType,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* pUserData)
		{
			// Select prefix depending on flags passed to the callback
			std::string prefix("");

			if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
				prefix = "VERBOSE: ";
			}
			else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
				prefix = "INFO: ";
			}
			else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
				prefix = "WARNING: ";
			}
			else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
				prefix = "ERROR: ";
			}


			// Display message to default output (console/logcat)
			std::stringstream debugMessage;
			debugMessage << prefix << "[" << pCallbackData->messageIdNumber << "][" << pCallbackData->pMessageIdName << "] : " << pCallbackData->pMessage;

			if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
				std::cerr << debugMessage.str() << "\n";
			} else {
				std::cout << debugMessage.str() << "\n";
			}
			fflush(stdout);


			// The return value of this callback controls whether the Vulkan call that caused the validation message will be aborted or not
			// We return VK_FALSE as we DON'T want Vulkan calls that cause a validation message to abort
			// If you instead want to have calls abort, pass in VK_TRUE and the function will return VK_ERROR_VALIDATION_FAILED_EXT 
			return VK_FALSE;
		}

		void setupDebugging(vk::Instance instance, vk::DebugReportFlagsEXT flags, vk::DebugReportCallbackEXT callBack)
		{

			vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{};
			debugUtilsMessengerCI.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
			debugUtilsMessengerCI.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
			debugUtilsMessengerCI.pfnUserCallback = debugUtilsMessengerCallback;
			debugUtilsMessenger = instance.createDebugUtilsMessengerEXTUnique(debugUtilsMessengerCI, nullptr, dynamicDispatchLoader);
		}

		void freeDebugCallback()
		{
            debugUtilsMessenger.reset();
		}
	}

	namespace debugmarker
	{
		bool active = false;

		void setup(vk::Device device)
		{
			// Set flag if at least one function pointer is present
			active = (dynamicDispatchLoader.vkDebugMarkerSetObjectTagEXT != VK_NULL_HANDLE);
		}

		void setObjectName(vk::Device device, uint64_t object, vk::DebugReportObjectTypeEXT objectType, const char *name)
		{
			// Check for valid function pointer (may not be present if not running in a debugging application)
			if (dynamicDispatchLoader.vkDebugMarkerSetObjectNameEXT)
			{
				vk::DebugMarkerObjectNameInfoEXT nameInfo = {};
				nameInfo.objectType = objectType;
				nameInfo.object = object;
				nameInfo.pObjectName = name;
				device.debugMarkerSetObjectNameEXT(nameInfo, dynamicDispatchLoader);
			}
		}

		void setObjectTag(vk::Device device, uint64_t object, vk::DebugReportObjectTypeEXT objectType, uint64_t name, size_t tagSize, const void* tag)
		{
			// Check for valid function pointer (may not be present if not running in a debugging application)
			if (dynamicDispatchLoader.vkDebugMarkerSetObjectTagEXT)
			{
				vk::DebugMarkerObjectTagInfoEXT tagInfo = {};
				tagInfo.objectType = objectType;
				tagInfo.object = object;
				tagInfo.tagName = name;
				tagInfo.tagSize = tagSize;
				tagInfo.pTag = tag;
				device.debugMarkerSetObjectTagEXT(tagInfo, dynamicDispatchLoader);
			}
		}

		void beginRegion(vk::CommandBuffer cmdbuffer, const char* pMarkerName, glm::vec4 color)
		{
			// Check for valid function pointer (may not be present if not running in a debugging application)
			if (dynamicDispatchLoader.vkCmdDebugMarkerBeginEXT)
			{
				vk::DebugMarkerMarkerInfoEXT markerInfo = {};
				memcpy(markerInfo.color, &color[0], sizeof(float) * 4);
				markerInfo.pMarkerName = pMarkerName;
				cmdbuffer.debugMarkerBeginEXT(markerInfo, dynamicDispatchLoader);
			}
		}

		void insert(vk::CommandBuffer cmdbuffer, std::string markerName, glm::vec4 color)
		{
			// Check for valid function pointer (may not be present if not running in a debugging application)
			if (dynamicDispatchLoader.vkCmdDebugMarkerInsertEXT)
			{
				vk::DebugMarkerMarkerInfoEXT markerInfo = {};
				memcpy(markerInfo.color, &color[0], sizeof(float) * 4);
				markerInfo.pMarkerName = markerName.c_str();
				cmdbuffer.debugMarkerInsertEXT(markerInfo, dynamicDispatchLoader);
			}
		}

		void endRegion(vk::CommandBuffer cmdBuffer)
		{
			// Check for valid function (may not be present if not running in a debugging application)
			if (dynamicDispatchLoader.vkCmdDebugMarkerEndEXT)
			{
				cmdBuffer.debugMarkerEndEXT(dynamicDispatchLoader);
			}
		}

		void setCommandBufferName(vk::Device device, vk::CommandBuffer cmdBuffer, const char * name)
		{
			setObjectName(device, (uint64_t)(VkCommandBuffer)cmdBuffer, vk::DebugReportObjectTypeEXT::eCommandBuffer, name);
		}

		void setQueueName(vk::Device device, vk::Queue queue, const char * name)
		{
			setObjectName(device, (uint64_t)(VkQueue)queue, vk::DebugReportObjectTypeEXT::eQueue, name);
		}

		void setImageName(vk::Device device, vk::Image image, const char * name)
		{
			setObjectName(device, (uint64_t)(VkImage)image, vk::DebugReportObjectTypeEXT::eImage, name);
		}

		void setSamplerName(vk::Device device, vk::Sampler sampler, const char * name)
		{
			setObjectName(device, (uint64_t)(VkSampler)sampler, vk::DebugReportObjectTypeEXT::eSampler, name);
		}

		void setBufferName(vk::Device device, vk::Buffer buffer, const char * name)
		{
			setObjectName(device, (uint64_t)(VkBuffer)buffer, vk::DebugReportObjectTypeEXT::eBuffer, name);
		}

		void setDeviceMemoryName(vk::Device device, vk::DeviceMemory memory, const char * name)
		{
			setObjectName(device, (uint64_t)(VkDeviceMemory)memory, vk::DebugReportObjectTypeEXT::eDeviceMemory, name);
		}

		void setShaderModuleName(vk::Device device, vk::ShaderModule shaderModule, const char * name)
		{
			setObjectName(device, (uint64_t)(VkShaderModule)shaderModule, vk::DebugReportObjectTypeEXT::eShaderModule, name);
		}

		void setPipelineName(vk::Device device, vk::Pipeline pipeline, const char * name)
		{
			setObjectName(device, (uint64_t)(VkPipeline)pipeline, vk::DebugReportObjectTypeEXT::ePipeline, name);
		}

		void setPipelineLayoutName(vk::Device device, vk::PipelineLayout pipelineLayout, const char * name)
		{
			setObjectName(device, (uint64_t)(VkPipelineLayout)pipelineLayout, vk::DebugReportObjectTypeEXT::ePipelineLayout, name);
		}

		void setRenderPassName(vk::Device device, vk::RenderPass renderPass, const char * name)
		{
			setObjectName(device, (uint64_t)(VkRenderPass)renderPass, vk::DebugReportObjectTypeEXT::eRenderPass, name);
		}

		void setFramebufferName(vk::Device device, vk::Framebuffer framebuffer, const char * name)
		{
			setObjectName(device, (uint64_t)(VkFramebuffer)framebuffer, vk::DebugReportObjectTypeEXT::eFramebuffer, name);
		}

		void setDescriptorSetLayoutName(vk::Device device, vk::DescriptorSetLayout descriptorSetLayout, const char * name)
		{
			setObjectName(device, (uint64_t)(VkDescriptorSetLayout)descriptorSetLayout, vk::DebugReportObjectTypeEXT::eDescriptorSetLayout, name);
		}

		void setDescriptorSetName(vk::Device device, vk::DescriptorSet descriptorSet, const char * name)
		{
			setObjectName(device, (uint64_t)(VkDescriptorSet)descriptorSet, vk::DebugReportObjectTypeEXT::eDescriptorSet, name);
		}

		void setSemaphoreName(vk::Device device, vk::Semaphore semaphore, const char * name)
		{
			setObjectName(device, (uint64_t)(VkSemaphore)semaphore, vk::DebugReportObjectTypeEXT::eSemaphore, name);
		}

		void setFenceName(vk::Device device, vk::Fence fence, const char * name)
		{
			setObjectName(device, (uint64_t)(VkFence)fence, vk::DebugReportObjectTypeEXT::eFence, name);
		}

		void setEventName(vk::Device device, vk::Event _event, const char * name)
		{
			setObjectName(device, (uint64_t)(VkEvent)_event, vk::DebugReportObjectTypeEXT::eEvent, name);
		}
	};
}

