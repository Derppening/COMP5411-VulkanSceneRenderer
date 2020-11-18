/*
* Vulkan buffer class
*
* Encapsulates a Vulkan buffer
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <vector>

#include "vulkan/vulkan.h"
#include "VulkanTools.h"

namespace vks
{	
	/**
	* @brief Encapsulates access to a Vulkan buffer backed up by device memory
	* @note To be filled by an external source like the VulkanDevice
	*/
	struct Buffer
	{
		vk::Device device;
		vk::UniqueBuffer buffer;
		vk::UniqueDeviceMemory memory;
		vk::DescriptorBufferInfo descriptor;
		vk::DeviceSize size = 0;
		vk::DeviceSize alignment = 0;
		void* mapped = nullptr;
		/** @brief Usage flags to be filled by external source at buffer creation (to query at some later point) */
		vk::BufferUsageFlags usageFlags;
		/** @brief Memory property flags to be filled by external source at buffer creation (to query at some later point) */
		vk::MemoryPropertyFlags memoryPropertyFlags;
		void map(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0);
		void unmap();
		void bind(vk::DeviceSize offset = 0);
		void setupDescriptor(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0);
		void copyTo(void* data, vk::DeviceSize size);
		void flush(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0);
		void invalidate(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0);
		void destroy();
	};
}