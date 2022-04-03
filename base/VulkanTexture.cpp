/*
* Vulkan texture loader
*
* Copyright(C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license(MIT) (http://opensource.org/licenses/MIT)
*/

#include <VulkanTexture.h>

namespace vks
{
	void Texture::updateDescriptor()
	{
		descriptor.sampler = *sampler;
		descriptor.imageView = *view;
		descriptor.imageLayout = imageLayout;
	}

	void Texture::destroy()
	{
		view.reset();
		image.reset();
		sampler.reset();
		deviceMemory.reset();
	}

	ktxResult Texture::loadKTXFile(std::string filename, ktxTexture **target)
	{
		ktxResult result = KTX_SUCCESS;
		if (!vks::tools::fileExists(filename)) {
			vks::tools::exitFatal("Could not load texture from " + filename + "\n\nThe file may be part of the additional asset pack.\n\nRun \"download_assets.py\" in the repository root to download the latest version.", -1);
		}
		result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, target);			
		return result;
	}

	/**
	* Load a 2D texture including all mip levels
	*
	* @param filename File to load (supports .ktx)
	* @param format Vulkan format of the image data stored in the file
	* @param device Vulkan device to create the texture on
	* @param copyQueue Queue used for the texture staging copy commands (must support transfer)
	* @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
	* @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	* @param (Optional) forceLinear Force linear tiling (not advised, defaults to false)
	*
	*/
	void Texture2D::loadFromFile(std::string filename, vk::Format format, vks::VulkanDevice *device, vk::Queue copyQueue, vk::ImageUsageFlags imageUsageFlags, vk::ImageLayout imageLayout, bool forceLinear)
	{
		ktxTexture* ktxTexture;
		ktxResult result = loadKTXFile(filename, &ktxTexture);
		assert(result == KTX_SUCCESS);

		this->device = device;
		width = ktxTexture->baseWidth;
		height = ktxTexture->baseHeight;
		mipLevels = ktxTexture->numLevels;

		ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
		ktx_size_t ktxTextureSize = ktxTexture_GetDataSize(ktxTexture);

		// Get device properties for the requested texture format
		vk::FormatProperties2 formatProperties = device->physicalDevice.getFormatProperties2(format);

		// Only use linear tiling if requested (and supported by the device)
		// Support for linear tiling is mostly limited, so prefer to use
		// optimal tiling instead
		// On most implementations linear tiling will only support a very
		// limited amount of formats and features (mip maps, cubemaps, arrays, etc.)
		vk::Bool32 useStaging = !forceLinear;

		vk::MemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		vk::MemoryRequirements2 memReqs;

		// Use a separate command buffer for texture loading
		vk::UniqueCommandBuffer copyCmd = device->createCommandBuffer(vk::CommandBufferLevel::ePrimary, true);

		if (useStaging)
		{
			// Create a host-visible staging buffer that contains the raw image data
			vk::UniqueBuffer stagingBuffer;
			vk::UniqueDeviceMemory stagingMemory;

			vk::BufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo();
			bufferCreateInfo.size = ktxTextureSize;
			// This buffer is used as a transfer source for the buffer copy
			bufferCreateInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
			bufferCreateInfo.sharingMode = vk::SharingMode::eExclusive;

			stagingBuffer = device->logicalDevice->createBufferUnique(bufferCreateInfo);

			// Get memory requirements for the staging buffer (alignment, memory type bits)
			memReqs = device->logicalDevice->getBufferMemoryRequirements2(*stagingBuffer);

			memAllocInfo.allocationSize = memReqs.memoryRequirements.size;
			// Get memory type index for a host visible buffer
			memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

			stagingMemory = device->logicalDevice->allocateMemoryUnique(memAllocInfo);
			device->logicalDevice->bindBufferMemory(*stagingBuffer, *stagingMemory, 0);

			// Copy texture data into staging buffer
			uint8_t *data;
			data = (uint8_t*)device->logicalDevice->mapMemory(*stagingMemory, 0, memReqs.memoryRequirements.size, {});
			std::copy_n(ktxTextureData, ktxTextureSize, data);
			device->logicalDevice->unmapMemory(*stagingMemory);

			// Setup buffer copy regions for each mip level
			std::vector<vk::BufferImageCopy> bufferCopyRegions;

			for (uint32_t i = 0; i < mipLevels; i++)
			{
				ktx_size_t offset;
				KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &offset);
				assert(result == KTX_SUCCESS);

				vk::BufferImageCopy bufferCopyRegion = {};
				bufferCopyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
				bufferCopyRegion.imageSubresource.mipLevel = i;
				bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
				bufferCopyRegion.imageSubresource.layerCount = 1;
				bufferCopyRegion.imageExtent.width = std::max(1u, ktxTexture->baseWidth >> i);
				bufferCopyRegion.imageExtent.height = std::max(1u, ktxTexture->baseHeight >> i);
				bufferCopyRegion.imageExtent.depth = 1;
				bufferCopyRegion.bufferOffset = offset;

				bufferCopyRegions.push_back(bufferCopyRegion);
			}

			// Create optimal tiled target image
			vk::ImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
			imageCreateInfo.imageType = vk::ImageType::e2D;
			imageCreateInfo.format = format;
			imageCreateInfo.mipLevels = mipLevels;
			imageCreateInfo.arrayLayers = 1;
			imageCreateInfo.samples = vk::SampleCountFlagBits::e1;
			imageCreateInfo.tiling = vk::ImageTiling::eOptimal;
			imageCreateInfo.sharingMode = vk::SharingMode::eExclusive;
			imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;
			imageCreateInfo.extent = vk::Extent3D{ width, height, 1 };
			imageCreateInfo.usage = imageUsageFlags;
			// Ensure that the TRANSFER_DST bit is set for staging
			if (!(imageCreateInfo.usage & vk::ImageUsageFlagBits::eTransferDst))
			{
				imageCreateInfo.usage |= vk::ImageUsageFlagBits::eTransferDst;
			}
			image = device->logicalDevice->createImageUnique(imageCreateInfo);

			memReqs = device->logicalDevice->getImageMemoryRequirements2(*image);

			memAllocInfo.allocationSize = memReqs.memoryRequirements.size;

			memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
			deviceMemory = device->logicalDevice->allocateMemoryUnique(memAllocInfo);
			device->logicalDevice->bindImageMemory(*image, *deviceMemory, 0);

			vk::ImageSubresourceRange subresourceRange = {};
			subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = mipLevels;
			subresourceRange.layerCount = 1;

			// Image barrier for optimal image (target)
			// Optimal image will be used as destination for the copy
			vks::tools::setImageLayout(
				*copyCmd,
				*image,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eTransferDstOptimal,
				subresourceRange);

			// Copy mip levels from staging buffer
          copyCmd->copyBufferToImage(*stagingBuffer, *image, vk::ImageLayout::eTransferDstOptimal, bufferCopyRegions);

			// Change texture image layout to shader read after all mip levels have been copied
			this->imageLayout = imageLayout;
			vks::tools::setImageLayout(
				*copyCmd,
				*image,
				vk::ImageLayout::eTransferDstOptimal,
				vk::ImageLayout(imageLayout),
				subresourceRange);

			device->flushCommandBuffer(copyCmd, copyQueue);

			// Clean up staging resources
			stagingMemory.reset();
			stagingBuffer.reset();
		}
		else
		{
			// Prefer using optimal tiling, as linear tiling 
			// may support only a small set of features 
			// depending on implementation (e.g. no mip maps, only one layer, etc.)

			// Check if this support is supported for linear tiling
			assert(formatProperties.formatProperties.linearTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage);

			vk::UniqueImage mappableImage;
			vk::UniqueDeviceMemory mappableMemory;

			vk::ImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
			imageCreateInfo.imageType = vk::ImageType::e2D;
			imageCreateInfo.format = format;
			imageCreateInfo.extent = vk::Extent3D{ width, height, 1 };
			imageCreateInfo.mipLevels = 1;
			imageCreateInfo.arrayLayers = 1;
			imageCreateInfo.samples = vk::SampleCountFlagBits::e1;
			imageCreateInfo.tiling = vk::ImageTiling::eLinear;
			imageCreateInfo.usage = imageUsageFlags;
			imageCreateInfo.sharingMode = vk::SharingMode::eExclusive;
			imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;

			// Load mip map level 0 to linear tiling image
			mappableImage = device->logicalDevice->createImageUnique(imageCreateInfo);

			// Get memory requirements for this image 
			// like size and alignment
			memReqs = device->logicalDevice->getImageMemoryRequirements2(*mappableImage);
			// Set memory allocation size to required memory size
			memAllocInfo.allocationSize = memReqs.memoryRequirements.size;

			// Get memory type that can be mapped to host memory
			memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

			// Allocate host memory
			mappableMemory = device->logicalDevice->allocateMemoryUnique(memAllocInfo);

			// Bind allocated image for use
			device->logicalDevice->bindImageMemory(*mappableImage, *mappableMemory, 0);

			// Get sub resource layout
			// Mip map count, array layer, etc.
			vk::ImageSubresource subRes = {};
			subRes.aspectMask = vk::ImageAspectFlagBits::eColor;
			subRes.mipLevel = 0;

			vk::SubresourceLayout subResLayout;
			void *data;

			// Get sub resources layout 
			// Includes row pitch, size offsets, etc.
			subResLayout = device->logicalDevice->getImageSubresourceLayout(*mappableImage, subRes);

			// Map image memory
			data = device->logicalDevice->mapMemory(*mappableMemory, 0, memReqs.memoryRequirements.size, {});

			// Copy image data into memory
			std::copy_n(reinterpret_cast<std::byte*>(ktxTextureData), memReqs.memoryRequirements.size, static_cast<std::byte*>(data));

			device->logicalDevice->unmapMemory(*mappableMemory);

			// Linear tiled images don't need to be staged
			// and can be directly used as textures
			image = std::move(mappableImage);
			deviceMemory = std::move(mappableMemory);
			this->imageLayout = imageLayout;

			// Setup image memory barrier
			vks::tools::setImageLayout(*copyCmd, *image, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::eUndefined, vk::ImageLayout(imageLayout));

			device->flushCommandBuffer(copyCmd, copyQueue);
		}

		ktxTexture_Destroy(ktxTexture);

		// Create a default sampler
		vk::SamplerCreateInfo samplerCreateInfo = {};
		samplerCreateInfo.magFilter = vk::Filter::eLinear;
		samplerCreateInfo.minFilter = vk::Filter::eLinear;
		samplerCreateInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
		samplerCreateInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
		samplerCreateInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
		samplerCreateInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.compareOp = vk::CompareOp::eNever;
		samplerCreateInfo.minLod = 0.0f;
		// Max level-of-detail should match mip level count
		samplerCreateInfo.maxLod = (useStaging) ? (float)mipLevels : 0.0f;
		// Only enable anisotropic filtering if enabled on the device
		samplerCreateInfo.maxAnisotropy = device->enabledFeatures.features.samplerAnisotropy ? device->properties.properties.limits.maxSamplerAnisotropy : 1.0f;
		samplerCreateInfo.anisotropyEnable = device->enabledFeatures.features.samplerAnisotropy;
		samplerCreateInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite;
		sampler = device->logicalDevice->createSamplerUnique(samplerCreateInfo);

		// Create image view
		// Textures are not directly accessed by the shaders and
		// are abstracted by image views containing additional
		// information and sub resource ranges
		vk::ImageViewCreateInfo viewCreateInfo = {};
		viewCreateInfo.viewType = vk::ImageViewType::e2D;
		viewCreateInfo.format = format;
		viewCreateInfo.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
		viewCreateInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
		// Linear tiling usually won't support mip maps
		// Only set mip map count if optimal tiling is used
		viewCreateInfo.subresourceRange.levelCount = (useStaging) ? mipLevels : 1;
		viewCreateInfo.image = *image;
		view = device->logicalDevice->createImageViewUnique(viewCreateInfo);

		// Update descriptor image info member that can be used for setting up descriptor sets
		updateDescriptor();
	}

	/**
	* Creates a 2D texture from a buffer
	*
	* @param buffer Buffer containing texture data to upload
	* @param bufferSize Size of the buffer in machine units
	* @param width Width of the texture to create
	* @param height Height of the texture to create
	* @param format Vulkan format of the image data stored in the file
	* @param device Vulkan device to create the texture on
	* @param copyQueue Queue used for the texture staging copy commands (must support transfer)
	* @param (Optional) filter Texture filtering for the sampler (defaults to VK_FILTER_LINEAR)
	* @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
	* @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	*/
	void Texture2D::fromBuffer(void* buffer, vk::DeviceSize bufferSize, vk::Format format, uint32_t texWidth, uint32_t texHeight, vks::VulkanDevice *device, vk::Queue copyQueue, vk::Filter filter, vk::ImageUsageFlags imageUsageFlags, vk::ImageLayout imageLayout)
	{
		assert(buffer);

		this->device = device;
		width = texWidth;
		height = texHeight;
		mipLevels = 1;

		vk::MemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		vk::MemoryRequirements2 memReqs;

		// Use a separate command buffer for texture loading
		vk::UniqueCommandBuffer copyCmd = device->createCommandBuffer(vk::CommandBufferLevel::ePrimary, true);

		// Create a host-visible staging buffer that contains the raw image data
		vk::UniqueBuffer stagingBuffer;
		vk::UniqueDeviceMemory stagingMemory;

		vk::BufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo();
		bufferCreateInfo.size = bufferSize;
		// This buffer is used as a transfer source for the buffer copy
		bufferCreateInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
		bufferCreateInfo.sharingMode = vk::SharingMode::eExclusive;

		stagingBuffer = device->logicalDevice->createBufferUnique(bufferCreateInfo);

		// Get memory requirements for the staging buffer (alignment, memory type bits)
		memReqs = device->logicalDevice->getBufferMemoryRequirements2(*stagingBuffer);

		memAllocInfo.allocationSize = memReqs.memoryRequirements.size;
		// Get memory type index for a host visible buffer
		memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

		stagingMemory = device->logicalDevice->allocateMemoryUnique(memAllocInfo);
		device->logicalDevice->bindBufferMemory(*stagingBuffer, *stagingMemory, 0);

		// Copy texture data into staging buffer
		uint8_t *data;
		data = (uint8_t*)device->logicalDevice->mapMemory(*stagingMemory, 0, memReqs.memoryRequirements.size, {});
		std::copy_n(static_cast<std::byte*>(buffer), bufferSize, reinterpret_cast<std::byte*>(data));
		device->logicalDevice->unmapMemory(*stagingMemory);

		vk::BufferImageCopy bufferCopyRegion = {};
		bufferCopyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		bufferCopyRegion.imageSubresource.mipLevel = 0;
		bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
		bufferCopyRegion.imageSubresource.layerCount = 1;
		bufferCopyRegion.imageExtent.width = width;
		bufferCopyRegion.imageExtent.height = height;
		bufferCopyRegion.imageExtent.depth = 1;
		bufferCopyRegion.bufferOffset = 0;

		// Create optimal tiled target image
		vk::ImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		imageCreateInfo.imageType = vk::ImageType::e2D;
		imageCreateInfo.format = format;
		imageCreateInfo.mipLevels = mipLevels;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = vk::SampleCountFlagBits::e1;
		imageCreateInfo.tiling = vk::ImageTiling::eOptimal;
		imageCreateInfo.sharingMode = vk::SharingMode::eExclusive;
		imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;
		imageCreateInfo.extent = vk::Extent3D{ width, height, 1 };
		imageCreateInfo.usage = imageUsageFlags;
		// Ensure that the TRANSFER_DST bit is set for staging
		if (!(imageCreateInfo.usage & vk::ImageUsageFlagBits::eTransferDst))
		{
			imageCreateInfo.usage |= vk::ImageUsageFlagBits::eTransferDst;
		}
		image = device->logicalDevice->createImageUnique(imageCreateInfo);

		memReqs = device->logicalDevice->getImageMemoryRequirements2(*image);

		memAllocInfo.allocationSize = memReqs.memoryRequirements.size;

		memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
		deviceMemory = device->logicalDevice->allocateMemoryUnique(memAllocInfo);
		device->logicalDevice->bindImageMemory(*image, *deviceMemory, 0);

		vk::ImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = mipLevels;
		subresourceRange.layerCount = 1;

		// Image barrier for optimal image (target)
		// Optimal image will be used as destination for the copy
		vks::tools::setImageLayout(
			*copyCmd,
			*image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal,
			subresourceRange);

		// Copy mip levels from staging buffer
		copyCmd->copyBufferToImage(*stagingBuffer, *image, vk::ImageLayout::eTransferDstOptimal, bufferCopyRegion);

		// Change texture image layout to shader read after all mip levels have been copied
		this->imageLayout = imageLayout;
		vks::tools::setImageLayout(
			*copyCmd,
			*image,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout(imageLayout),
			subresourceRange);

		device->flushCommandBuffer(copyCmd, copyQueue);

		// Clean up staging resources
		stagingMemory.reset();
		stagingBuffer.reset();

		// Create sampler
		vk::SamplerCreateInfo samplerCreateInfo = {};
		samplerCreateInfo.magFilter = filter;
		samplerCreateInfo.minFilter = filter;
		samplerCreateInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
		samplerCreateInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
		samplerCreateInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
		samplerCreateInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.compareOp = vk::CompareOp::eNever;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 0.0f;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		sampler = device->logicalDevice->createSamplerUnique(samplerCreateInfo);

		// Create image view
		vk::ImageViewCreateInfo viewCreateInfo = {};
		viewCreateInfo.pNext = NULL;
		viewCreateInfo.viewType = vk::ImageViewType::e2D;
		viewCreateInfo.format = format;
		viewCreateInfo.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
		viewCreateInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
		viewCreateInfo.subresourceRange.levelCount = 1;
		viewCreateInfo.image = *image;
		view = device->logicalDevice->createImageViewUnique(viewCreateInfo);

		// Update descriptor image info member that can be used for setting up descriptor sets
		updateDescriptor();
	}

	/**
	* Load a 2D texture array including all mip levels
	*
	* @param filename File to load (supports .ktx)
	* @param format Vulkan format of the image data stored in the file
	* @param device Vulkan device to create the texture on
	* @param copyQueue Queue used for the texture staging copy commands (must support transfer)
	* @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
	* @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	*
	*/
	void Texture2DArray::loadFromFile(std::string filename, vk::Format format, vks::VulkanDevice *device, vk::Queue copyQueue, vk::ImageUsageFlags imageUsageFlags, vk::ImageLayout imageLayout)
	{
		ktxTexture* ktxTexture;
		ktxResult result = loadKTXFile(filename, &ktxTexture);
		assert(result == KTX_SUCCESS);

		this->device = device;
		width = ktxTexture->baseWidth;
		height = ktxTexture->baseHeight;
		layerCount = ktxTexture->numLayers;
		mipLevels = ktxTexture->numLevels;

		ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
		ktx_size_t ktxTextureSize = ktxTexture_GetDataSize(ktxTexture);

		vk::MemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		vk::MemoryRequirements2 memReqs;

		// Create a host-visible staging buffer that contains the raw image data
		vk::UniqueBuffer stagingBuffer;
		vk::UniqueDeviceMemory stagingMemory;

		vk::BufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo();
		bufferCreateInfo.size = ktxTextureSize;
		// This buffer is used as a transfer source for the buffer copy
		bufferCreateInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
		bufferCreateInfo.sharingMode = vk::SharingMode::eExclusive;

		stagingBuffer = device->logicalDevice->createBufferUnique(bufferCreateInfo);

		// Get memory requirements for the staging buffer (alignment, memory type bits)
		memReqs = device->logicalDevice->getBufferMemoryRequirements2(*stagingBuffer);

		memAllocInfo.allocationSize = memReqs.memoryRequirements.size;
		// Get memory type index for a host visible buffer
		memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

		stagingMemory = device->logicalDevice->allocateMemoryUnique(memAllocInfo);
		device->logicalDevice->bindBufferMemory(*stagingBuffer, *stagingMemory, 0);

		// Copy texture data into staging buffer
		uint8_t *data;
		data = (uint8_t*)device->logicalDevice->mapMemory(*stagingMemory, 0, memReqs.memoryRequirements.size, {});
		std::copy_n(ktxTextureData, ktxTextureSize, data);
		device->logicalDevice->unmapMemory(*stagingMemory);

		// Setup buffer copy regions for each layer including all of its miplevels
		std::vector<vk::BufferImageCopy> bufferCopyRegions;

		for (uint32_t layer = 0; layer < layerCount; layer++)
		{
			for (uint32_t level = 0; level < mipLevels; level++)
			{
				ktx_size_t offset;
				KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, level, layer, 0, &offset);
				assert(result == KTX_SUCCESS);

				vk::BufferImageCopy bufferCopyRegion = {};
				bufferCopyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
				bufferCopyRegion.imageSubresource.mipLevel = level;
				bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
				bufferCopyRegion.imageSubresource.layerCount = 1;
				bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> level;
				bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> level;
				bufferCopyRegion.imageExtent.depth = 1;
				bufferCopyRegion.bufferOffset = offset;

				bufferCopyRegions.push_back(bufferCopyRegion);
			}
		}

		// Create optimal tiled target image
		vk::ImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		imageCreateInfo.imageType = vk::ImageType::e2D;
		imageCreateInfo.format = format;
		imageCreateInfo.samples = vk::SampleCountFlagBits::e1;
		imageCreateInfo.tiling = vk::ImageTiling::eOptimal;
		imageCreateInfo.sharingMode = vk::SharingMode::eExclusive;
		imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;
		imageCreateInfo.extent = vk::Extent3D{ width, height, 1 };
		imageCreateInfo.usage = imageUsageFlags;
		// Ensure that the TRANSFER_DST bit is set for staging
		if (!(imageCreateInfo.usage & vk::ImageUsageFlagBits::eTransferDst))
		{
			imageCreateInfo.usage |= vk::ImageUsageFlagBits::eTransferDst;
		}
		imageCreateInfo.arrayLayers = layerCount;
		imageCreateInfo.mipLevels = mipLevels;

		image = device->logicalDevice->createImageUnique(imageCreateInfo);

		memReqs = device->logicalDevice->getImageMemoryRequirements2(*image);

		memAllocInfo.allocationSize = memReqs.memoryRequirements.size;
		memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

		deviceMemory = device->logicalDevice->allocateMemoryUnique(memAllocInfo);
		device->logicalDevice->bindImageMemory(*image, *deviceMemory, 0);

		// Use a separate command buffer for texture loading
		vk::UniqueCommandBuffer copyCmd = device->createCommandBuffer(vk::CommandBufferLevel::ePrimary, true);

		// Image barrier for optimal image (target)
		// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
		vk::ImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = mipLevels;
		subresourceRange.layerCount = layerCount;

		vks::tools::setImageLayout(
			*copyCmd,
			*image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal,
			subresourceRange);

		// Copy the layers and mip levels from the staging buffer to the optimal tiled image
		copyCmd->copyBufferToImage(*stagingBuffer, *image, vk::ImageLayout::eTransferDstOptimal, bufferCopyRegions);

		// Change texture image layout to shader read after all faces have been copied
		this->imageLayout = imageLayout;
		vks::tools::setImageLayout(
			*copyCmd,
			*image,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout(imageLayout),
			subresourceRange);

		device->flushCommandBuffer(copyCmd, copyQueue);

		// Create sampler
		vk::SamplerCreateInfo samplerCreateInfo = vks::initializers::samplerCreateInfo();
		samplerCreateInfo.magFilter = vk::Filter::eLinear;
		samplerCreateInfo.minFilter = vk::Filter::eLinear;
		samplerCreateInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
		samplerCreateInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
		samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
		samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.maxAnisotropy = device->enabledFeatures.features.samplerAnisotropy ? device->properties.properties.limits.maxSamplerAnisotropy : 1.0f;
		samplerCreateInfo.anisotropyEnable = device->enabledFeatures.features.samplerAnisotropy;
		samplerCreateInfo.compareOp = vk::CompareOp::eNever;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = (float)mipLevels;
		samplerCreateInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite;
		sampler = device->logicalDevice->createSamplerUnique(samplerCreateInfo);

		// Create image view
		vk::ImageViewCreateInfo viewCreateInfo = vks::initializers::imageViewCreateInfo();
		viewCreateInfo.viewType = vk::ImageViewType::e2DArray;
		viewCreateInfo.format = format;
		viewCreateInfo.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };;
		viewCreateInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
		viewCreateInfo.subresourceRange.layerCount = layerCount;
		viewCreateInfo.subresourceRange.levelCount = mipLevels;
		viewCreateInfo.image = *image;
		view = device->logicalDevice->createImageViewUnique(viewCreateInfo);

		// Clean up staging resources
		ktxTexture_Destroy(ktxTexture);
		stagingMemory.reset();
		stagingBuffer.reset();

		// Update descriptor image info member that can be used for setting up descriptor sets
		updateDescriptor();
	}

	/**
	* Load a cubemap texture including all mip levels from a single file
	*
	* @param filename File to load (supports .ktx)
	* @param format Vulkan format of the image data stored in the file
	* @param device Vulkan device to create the texture on
	* @param copyQueue Queue used for the texture staging copy commands (must support transfer)
	* @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
	* @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	*
	*/
	void TextureCubeMap::loadFromFile(std::string filename, vk::Format format, vks::VulkanDevice *device, vk::Queue copyQueue, vk::ImageUsageFlags imageUsageFlags, vk::ImageLayout imageLayout)
	{
		ktxTexture* ktxTexture;
		ktxResult result = loadKTXFile(filename, &ktxTexture);
		assert(result == KTX_SUCCESS);

		this->device = device;
		width = ktxTexture->baseWidth;
		height = ktxTexture->baseHeight;
		mipLevels = ktxTexture->numLevels;

		ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
		ktx_size_t ktxTextureSize = ktxTexture_GetDataSize(ktxTexture);

		vk::MemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		vk::MemoryRequirements2 memReqs;

		// Create a host-visible staging buffer that contains the raw image data
		vk::UniqueBuffer stagingBuffer;
		vk::UniqueDeviceMemory stagingMemory;

		vk::BufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo();
		bufferCreateInfo.size = ktxTextureSize;
		// This buffer is used as a transfer source for the buffer copy
		bufferCreateInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
		bufferCreateInfo.sharingMode = vk::SharingMode::eExclusive;

		stagingBuffer = device->logicalDevice->createBufferUnique(bufferCreateInfo);

		// Get memory requirements for the staging buffer (alignment, memory type bits)
		memReqs = device->logicalDevice->getBufferMemoryRequirements2(*stagingBuffer);

		memAllocInfo.allocationSize = memReqs.memoryRequirements.size;
		// Get memory type index for a host visible buffer
		memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

		stagingMemory = device->logicalDevice->allocateMemoryUnique(memAllocInfo);
		device->logicalDevice->bindBufferMemory(*stagingBuffer, *stagingMemory, 0);

		// Copy texture data into staging buffer
		uint8_t *data;
		data = (uint8_t*)device->logicalDevice->mapMemory(*stagingMemory, 0, memReqs.memoryRequirements.size, {});
		std::copy_n(ktxTextureData, ktxTextureSize, data);
		device->logicalDevice->unmapMemory(*stagingMemory);

		// Setup buffer copy regions for each face including all of its mip levels
		std::vector<vk::BufferImageCopy> bufferCopyRegions;

		for (uint32_t face = 0; face < 6; face++)
		{
			for (uint32_t level = 0; level < mipLevels; level++)
			{
				ktx_size_t offset;
				KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, level, 0, face, &offset);
				assert(result == KTX_SUCCESS);

				vk::BufferImageCopy bufferCopyRegion = {};
				bufferCopyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
				bufferCopyRegion.imageSubresource.mipLevel = level;
				bufferCopyRegion.imageSubresource.baseArrayLayer = face;
				bufferCopyRegion.imageSubresource.layerCount = 1;
				bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> level;
				bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> level;
				bufferCopyRegion.imageExtent.depth = 1;
				bufferCopyRegion.bufferOffset = offset;

				bufferCopyRegions.push_back(bufferCopyRegion);
			}
		}

		// Create optimal tiled target image
		vk::ImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		imageCreateInfo.imageType = vk::ImageType::e2D;
		imageCreateInfo.format = format;
		imageCreateInfo.mipLevels = mipLevels;
		imageCreateInfo.samples = vk::SampleCountFlagBits::e1;
		imageCreateInfo.tiling = vk::ImageTiling::eOptimal;
		imageCreateInfo.sharingMode = vk::SharingMode::eExclusive;
		imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;
		imageCreateInfo.extent = vk::Extent3D{ width, height, 1 };
		imageCreateInfo.usage = imageUsageFlags;
		// Ensure that the TRANSFER_DST bit is set for staging
		if (!(imageCreateInfo.usage & vk::ImageUsageFlagBits::eTransferDst))
		{
			imageCreateInfo.usage |= vk::ImageUsageFlagBits::eTransferDst;
		}
		// Cube faces count as array layers in Vulkan
		imageCreateInfo.arrayLayers = 6;
		// This flag is required for cube map images
		imageCreateInfo.flags = vk::ImageCreateFlagBits::eCubeCompatible;


		image = device->logicalDevice->createImageUnique(imageCreateInfo);

		memReqs = device->logicalDevice->getImageMemoryRequirements2(*image);

		memAllocInfo.allocationSize = memReqs.memoryRequirements.size;
		memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

		deviceMemory = device->logicalDevice->allocateMemoryUnique(memAllocInfo);
		device->logicalDevice->bindImageMemory(*image, *deviceMemory, 0);

		// Use a separate command buffer for texture loading
		vk::UniqueCommandBuffer copyCmd = device->createCommandBuffer(vk::CommandBufferLevel::ePrimary, true);

		// Image barrier for optimal image (target)
		// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
		vk::ImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = mipLevels;
		subresourceRange.layerCount = 6;

		vks::tools::setImageLayout(
			*copyCmd,
			*image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal,
			subresourceRange);

		// Copy the cube map faces from the staging buffer to the optimal tiled image
		copyCmd->copyBufferToImage(*stagingBuffer, *image, vk::ImageLayout::eTransferDstOptimal, bufferCopyRegions);

		// Change texture image layout to shader read after all faces have been copied
		this->imageLayout = imageLayout;
		vks::tools::setImageLayout(
			*copyCmd,
			*image,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout(imageLayout),
			subresourceRange);

		device->flushCommandBuffer(copyCmd, copyQueue);

		// Create sampler
		vk::SamplerCreateInfo samplerCreateInfo = vks::initializers::samplerCreateInfo();
		samplerCreateInfo.magFilter = vk::Filter::eLinear;
		samplerCreateInfo.minFilter = vk::Filter::eLinear;
		samplerCreateInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
		samplerCreateInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
		samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
		samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.maxAnisotropy = device->enabledFeatures.features.samplerAnisotropy ? device->properties.properties.limits.maxSamplerAnisotropy : 1.0f;
		samplerCreateInfo.anisotropyEnable = device->enabledFeatures.features.samplerAnisotropy;
		samplerCreateInfo.compareOp = vk::CompareOp::eNever;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = (float)mipLevels;
		samplerCreateInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite;
		sampler = device->logicalDevice->createSamplerUnique(samplerCreateInfo);

		// Create image view
		vk::ImageViewCreateInfo viewCreateInfo = vks::initializers::imageViewCreateInfo();
		viewCreateInfo.viewType = vk::ImageViewType::eCube;
		viewCreateInfo.format = format;
		viewCreateInfo.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };;
		viewCreateInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
		viewCreateInfo.subresourceRange.layerCount = 6;
		viewCreateInfo.subresourceRange.levelCount = mipLevels;
		viewCreateInfo.image = *image;
		view = device->logicalDevice->createImageViewUnique(viewCreateInfo);

		// Clean up staging resources
		ktxTexture_Destroy(ktxTexture);
		stagingMemory.reset();
		stagingMemory.reset();

		// Update descriptor image info member that can be used for setting up descriptor sets
		updateDescriptor();
	}

}
