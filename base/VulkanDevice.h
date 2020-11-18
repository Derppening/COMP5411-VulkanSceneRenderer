/*
* Vulkan device class
*
* Encapsulates a physical Vulkan device and its logical representation
*
* Copyright (C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include "VulkanBuffer.h"
#include "VulkanTools.h"
#include "vulkan/vulkan.h"
#include <algorithm>
#include <assert.h>
#include <exception>

namespace vks
{
struct VulkanDevice
{
	/** @brief Physical device representation */
	vk::PhysicalDevice physicalDevice;
	/** @brief Logical device representation (application's view of the device) */
	vk::UniqueDevice logicalDevice;
	/** @brief Properties of the physical device including limits that the application can check against */
	vk::PhysicalDeviceProperties properties;
	/** @brief Features of the physical device that an application can use to check if a feature is supported */
	vk::PhysicalDeviceFeatures features;
	/** @brief Features that have been enabled for use on the physical device */
	vk::PhysicalDeviceFeatures enabledFeatures;
	/** @brief Memory types and heaps of the physical device */
	vk::PhysicalDeviceMemoryProperties memoryProperties;
	/** @brief Queue family properties of the physical device */
	std::vector<vk::QueueFamilyProperties> queueFamilyProperties;
	/** @brief List of extensions supported by the device */
	std::vector<std::string> supportedExtensions;
	/** @brief Default command pool for the graphics queue family index */
	vk::CommandPool commandPool;
	/** @brief Set to true when the debug marker extension is detected */
	bool enableDebugMarkers = false;
	/** @brief Contains queue family indices */
	struct
	{
		uint32_t graphics;
		uint32_t compute;
		uint32_t transfer;
	} queueFamilyIndices;
	operator vk::Device() const
	{
		return *logicalDevice;
	};
	explicit VulkanDevice(vk::PhysicalDevice physicalDevice);
	~VulkanDevice();
	uint32_t        getMemoryType(uint32_t typeBits, vk::MemoryPropertyFlags properties, vk::Bool32 *memTypeFound = nullptr) const;
	uint32_t        getQueueFamilyIndex(vk::QueueFlagBits queueFlags) const;
	vk::Result        createLogicalDevice(vk::PhysicalDeviceFeatures enabledFeatures, std::vector<const char *> enabledExtensions, void *pNextChain, bool useSwapChain = true, vk::QueueFlags requestedQueueTypes = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute);
	vk::Result        createBuffer(vk::BufferUsageFlags usageFlags, vk::MemoryPropertyFlags memoryPropertyFlags, vk::DeviceSize size, vk::Buffer *buffer, vk::DeviceMemory *memory, void *data = nullptr);
	vk::Result        createBuffer(vk::BufferUsageFlags usageFlags, vk::MemoryPropertyFlags memoryPropertyFlags, vks::Buffer *buffer, vk::DeviceSize size, void *data = nullptr);
	void            copyBuffer(vks::Buffer *src, vks::Buffer *dst, vk::Queue queue, vk::BufferCopy *copyRegion = nullptr);
	vk::CommandPool   createCommandPool(uint32_t queueFamilyIndex, vk::CommandPoolCreateFlags createFlags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
	vk::CommandBuffer createCommandBuffer(vk::CommandBufferLevel level, vk::CommandPool pool, bool begin = false);
	vk::CommandBuffer createCommandBuffer(vk::CommandBufferLevel level, bool begin = false);
	void            flushCommandBuffer(vk::CommandBuffer commandBuffer, vk::Queue queue, vk::CommandPool pool, bool free = true);
	void            flushCommandBuffer(vk::CommandBuffer commandBuffer, vk::Queue queue, bool free = true);
	bool            extensionSupported(std::string extension);
	vk::Format        getSupportedDepthFormat(bool checkSamplingSupport);
};
}        // namespace vks
