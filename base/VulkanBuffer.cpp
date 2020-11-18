/*
* Vulkan buffer class
*
* Encapsulates a Vulkan buffer
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "VulkanBuffer.h"

namespace vks
{	
	/** 
	* Map a memory range of this buffer. If successful, mapped points to the specified buffer range.
	* 
	* @param size (Optional) Size of the memory range to map. Pass VK_WHOLE_SIZE to map the complete buffer range.
	* @param offset (Optional) Byte offset from beginning
	*/
	void Buffer::map(vk::DeviceSize size, vk::DeviceSize offset)
	{
		mapped = device.mapMemory(*memory, offset, size, {});
	}

	/**
	* Unmap a mapped memory range
	*
	* @note Does not return a result as vkUnmapMemory can't fail
	*/
	void Buffer::unmap()
	{
		if (mapped)
		{
			device.unmapMemory(*memory);
			mapped = nullptr;
		}
	}

	/** 
	* Attach the allocated memory block to the buffer
	* 
	* @param offset (Optional) Byte offset (from the beginning) for the memory region to bind
	* 
	* @return vk::Result of the bindBufferMemory call
	*/
	void Buffer::bind(vk::DeviceSize offset)
	{
		device.bindBufferMemory(*buffer, *memory, offset);
	}

	/**
	* Setup the default descriptor for this buffer
	*
	* @param size (Optional) Size of the memory range of the descriptor
	* @param offset (Optional) Byte offset from beginning
	*
	*/
	void Buffer::setupDescriptor(vk::DeviceSize size, vk::DeviceSize offset)
	{
		descriptor.offset = offset;
		descriptor.buffer = *buffer;
		descriptor.range = size;
	}

	/**
	* Copies the specified data to the mapped buffer
	* 
	* @param data Pointer to the data to copy
	* @param size Size of the data to copy in machine units
	*
	*/
	void Buffer::copyTo(void* data, vk::DeviceSize size)
	{
		assert(mapped);
		std::copy_n(static_cast<std::byte*>(data), size, static_cast<std::byte*>(mapped));
	}

	/** 
	* Flush a memory range of the buffer to make it visible to the device
	*
	* @note Only required for non-coherent memory
	*
	* @param size (Optional) Size of the memory range to flush. Pass VK_WHOLE_SIZE to flush the complete buffer range.
	* @param offset (Optional) Byte offset from beginning
	*
	* @return vk::Result of the flush call
	*/
	void Buffer::flush(vk::DeviceSize size, vk::DeviceSize offset)
	{
		vk::MappedMemoryRange mappedRange = {};
		mappedRange.memory = *memory;
		mappedRange.offset = offset;
		mappedRange.size = size;
		device.flushMappedMemoryRanges({mappedRange});
	}

	/**
	* Invalidate a memory range of the buffer to make it visible to the host
	*
	* @note Only required for non-coherent memory
	*
	* @param size (Optional) Size of the memory range to invalidate. Pass VK_WHOLE_SIZE to invalidate the complete buffer range.
	* @param offset (Optional) Byte offset from beginning
	*
	* @return vk::Result of the invalidate call
	*/
	void Buffer::invalidate(vk::DeviceSize size, vk::DeviceSize offset)
	{
		vk::MappedMemoryRange mappedRange = {};
		mappedRange.memory = *memory;
		mappedRange.offset = offset;
		mappedRange.size = size;
		device.invalidateMappedMemoryRanges({mappedRange});
	}

	/** 
	* Release all Vulkan resources held by this buffer
	*/
	void Buffer::destroy()
	{
		if (buffer)
		{
			buffer.reset();
		}
		if (memory)
		{
			memory.reset();
		}
	}
};
