/*
* Class wrapping access to the swap chain
* 
* A swap chain is a collection of framebuffers used for rendering and presentation to the windowing system
*
* Copyright (C) 2016-2021 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "VulkanSwapChain.h"

#include <vulkan/vulkan.hpp>

/** @brief Creates the platform specific surface abstraction of the native platform window used for presentation */	
void VulkanSwapChain::initSurface(GLFWwindow* window)
{
	VkResult err = VK_SUCCESS;
	VkSurfaceKHR surface;

	// Create the os-specific surface
	err = glfwCreateWindowSurface(instance, window, nullptr, &surface);

	this->surface = vk::UniqueSurfaceKHR{surface, instance};

	if (err != VK_SUCCESS) {
		vks::tools::exitFatal("Could not create surface!", err);
	}

	// Get available queue family properties
	std::vector<vk::QueueFamilyProperties2> queueProps = physicalDevice.getQueueFamilyProperties2();
    uint32_t queueCount = queueProps.size();

	// Iterate over each queue to learn whether it supports presenting:
	// Find a queue with present support
	// Will be used to present the swap chain images to the windowing system
	std::vector<vk::Bool32> supportsPresent(queueCount);
	for (uint32_t i = 0; i < queueCount; i++) 
	{
		supportsPresent[i] = physicalDevice.getSurfaceSupportKHR(i, *this->surface, vks::dynamicDispatchLoader);
//        supportsPresent[i] = physicalDevice.getSurfaceSupportKHR(i, *this->surface);
	}

	// Search for a graphics and a present queue in the array of queue
	// families, try to find one that supports both
	uint32_t graphicsQueueNodeIndex = UINT32_MAX;
	uint32_t presentQueueNodeIndex = UINT32_MAX;
	for (uint32_t i = 0; i < queueCount; i++) 
	{
		if ((queueProps[i].queueFamilyProperties.queueFlags & vk::QueueFlagBits::eGraphics))
		{
			if (graphicsQueueNodeIndex == UINT32_MAX) 
			{
				graphicsQueueNodeIndex = i;
			}

			if (supportsPresent[i] == VK_TRUE) 
			{
				graphicsQueueNodeIndex = i;
				presentQueueNodeIndex = i;
				break;
			}
		}
	}
	if (presentQueueNodeIndex == UINT32_MAX) 
	{	
		// If there's no queue that supports both present and graphics
		// try to find a separate present queue
		for (uint32_t i = 0; i < queueCount; ++i) 
		{
			if (supportsPresent[i] == VK_TRUE) 
			{
				presentQueueNodeIndex = i;
				break;
			}
		}
	}

	// Exit if either a graphics or a presenting queue hasn't been found
	if (graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX) 
	{
		vks::tools::exitFatal("Could not find a graphics and/or presenting queue!", -1);
	}

	// todo : Add support for separate graphics and presenting queue
	if (graphicsQueueNodeIndex != presentQueueNodeIndex) 
	{
		vks::tools::exitFatal("Separate graphics and presenting queues are not supported yet!", -1);
	}

	queueNodeIndex = graphicsQueueNodeIndex;

	// Get list of supported surface formats
	std::vector<vk::SurfaceFormatKHR> surfaceFormats = physicalDevice.getSurfaceFormatsKHR(*this->surface, vks::dynamicDispatchLoader);
//    std::vector<vk::SurfaceFormatKHR> surfaceFormats = physicalDevice.getSurfaceFormatsKHR(*this->surface);
    uint32_t formatCount = surfaceFormats.size();

	// If the surface format list only includes one entry with VK_FORMAT_UNDEFINED,
	// there is no preferred format, so we assume VK_FORMAT_B8G8R8A8_UNORM
	if ((formatCount == 1) && (surfaceFormats[0].format == vk::Format::eUndefined))
	{
		colorFormat = vk::Format::eB8G8R8A8Unorm;
		colorSpace = surfaceFormats[0].colorSpace;
	}
	else
	{
		// iterate over the list of available surface format and
		// check for the presence of VK_FORMAT_B8G8R8A8_UNORM
		bool found_B8G8R8A8_UNORM = false;
		for (auto&& surfaceFormat : surfaceFormats)
		{
			if (surfaceFormat.format == vk::Format::eB8G8R8A8Unorm)
			{
				colorFormat = surfaceFormat.format;
				colorSpace = surfaceFormat.colorSpace;
				found_B8G8R8A8_UNORM = true;
				break;
			}
		}

		// in case VK_FORMAT_B8G8R8A8_UNORM is not available
		// select the first available color format
		if (!found_B8G8R8A8_UNORM)
		{
			colorFormat = surfaceFormats[0].format;
			colorSpace = surfaceFormats[0].colorSpace;
		}
	}

}

/**
* Set instance, physical and logical device to use for the swapchain and get all required function pointers
* 
* @param instance Vulkan instance to use
* @param physicalDevice Physical device used to query properties and formats relevant to the swapchain
* @param device Logical representation of the device to create the swapchain for
*
*/
void VulkanSwapChain::connect(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device)
{
	this->instance = instance;
	this->physicalDevice = physicalDevice;
	this->device = device;
}

/** 
* Create the swapchain and get its images with given width and height
* 
* @param width Pointer to the width of the swapchain (may be adjusted to fit the requirements of the swapchain)
* @param height Pointer to the height of the swapchain (may be adjusted to fit the requirements of the swapchain)
* @param vsync (Optional) Can be used to force vsync-ed rendering (by using VK_PRESENT_MODE_FIFO_KHR as presentation mode)
*/
void VulkanSwapChain::create(uint32_t *width, uint32_t *height, bool vsync)
{
    // Store the current swap chain handle so we can use it later on to ease up recreation
    vk::SwapchainKHR oldSwapchain = *swapChain;

	// Get physical device surface properties and formats
	vk::SurfaceCapabilitiesKHR surfCaps = physicalDevice.getSurfaceCapabilitiesKHR(*surface, vks::dynamicDispatchLoader);
//    vk::SurfaceCapabilitiesKHR surfCaps = physicalDevice.getSurfaceCapabilitiesKHR(*surface);

	// Get available present modes
	std::vector<vk::PresentModeKHR> presentModes = physicalDevice.getSurfacePresentModesKHR(*surface, vks::dynamicDispatchLoader);
//    std::vector<vk::PresentModeKHR> presentModes = physicalDevice.getSurfacePresentModesKHR(*surface);
    uint32_t presentModeCount = presentModes.size();

	vk::Extent2D swapchainExtent = {};
	// If width (and height) equals the special value 0xFFFFFFFF, the size of the surface will be set by the swapchain
	if (surfCaps.currentExtent.width == (uint32_t)-1)
	{
		// If the surface size is undefined, the size is set to
		// the size of the images requested.
		swapchainExtent.width = *width;
		swapchainExtent.height = *height;
	}
	else
	{
		// If the surface size is defined, the swap chain size must match
		swapchainExtent = surfCaps.currentExtent;
		*width = surfCaps.currentExtent.width;
		*height = surfCaps.currentExtent.height;
	}


	// Select a present mode for the swapchain

	// The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
	// This mode waits for the vertical blank ("v-sync")
	vk::PresentModeKHR swapchainPresentMode = vk::PresentModeKHR::eFifo;

	// If v-sync is not requested, try to find a mailbox mode
	// It's the lowest latency non-tearing present mode available
	if (!vsync)
	{
		for (size_t i = 0; i < presentModeCount; i++)
		{
			if (presentModes[i] == vk::PresentModeKHR::eMailbox)
			{
				swapchainPresentMode = vk::PresentModeKHR::eMailbox;
				break;
			}
			if (presentModes[i] == vk::PresentModeKHR::eImmediate)
			{
				swapchainPresentMode = vk::PresentModeKHR::eImmediate;
			}
		}
	}

	// Determine the number of images
	uint32_t desiredNumberOfSwapchainImages = surfCaps.minImageCount + 1;
	if ((surfCaps.maxImageCount > 0) && (desiredNumberOfSwapchainImages > surfCaps.maxImageCount))
	{
		desiredNumberOfSwapchainImages = surfCaps.maxImageCount;
	}

	// Find the transformation of the surface
	vk::SurfaceTransformFlagBitsKHR preTransform;
	if (surfCaps.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity)
	{
		// We prefer a non-rotated transform
		preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
	}
	else
	{
		preTransform = surfCaps.currentTransform;
	}

	// Find a supported composite alpha format (not all devices support alpha opaque)
	vk::CompositeAlphaFlagBitsKHR compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	// Simply select the first composite alpha format available
	std::vector<vk::CompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
		vk::CompositeAlphaFlagBitsKHR::eOpaque,
		vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
		vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
		vk::CompositeAlphaFlagBitsKHR::eInherit,
	};
	for (auto& compositeAlphaFlag : compositeAlphaFlags) {
		if (surfCaps.supportedCompositeAlpha & compositeAlphaFlag) {
			compositeAlpha = compositeAlphaFlag;
			break;
		};
	}

	vk::SwapchainCreateInfoKHR swapchainCI = {};
	swapchainCI.surface = *surface;
	swapchainCI.minImageCount = desiredNumberOfSwapchainImages;
	swapchainCI.imageFormat = colorFormat;
	swapchainCI.imageColorSpace = colorSpace;
	swapchainCI.imageExtent = vk::Extent2D{ swapchainExtent.width, swapchainExtent.height };
	swapchainCI.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
	swapchainCI.preTransform = preTransform;
	swapchainCI.imageArrayLayers = 1;
	swapchainCI.imageSharingMode = vk::SharingMode::eExclusive;
	swapchainCI.queueFamilyIndexCount = 0;
	swapchainCI.presentMode = swapchainPresentMode;
    // Setting oldSwapChain to the saved handle of the previous swapchain aids in resource reuse and makes sure that we can still present already acquired images
      swapchainCI.oldSwapchain = oldSwapchain;
	// Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
	swapchainCI.clipped = VK_TRUE;
	swapchainCI.compositeAlpha = compositeAlpha;

	// Enable transfer source on swap chain images if supported
	if (surfCaps.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc) {
		swapchainCI.imageUsage |= vk::ImageUsageFlagBits::eTransferSrc;
	}

	// Enable transfer destination on swap chain images if supported
	if (surfCaps.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferDst) {
		swapchainCI.imageUsage |= vk::ImageUsageFlagBits::eTransferDst;
	}

	// If an existing swap chain is re-created, destroy the old swap chain
	// This also cleans up all the presentable images
	if (oldSwapchain)
	{ 
		for (uint32_t i = 0; i < imageCount; i++)
		{
			buffers[i].view.reset();
		}
	}

    swapChain = device.createSwapchainKHRUnique(swapchainCI, nullptr, vks::dynamicDispatchLoader);
//    swapChain = device.createSwapchainKHRUnique(swapchainCI);

  // Get the swap chain images
	images = device.getSwapchainImagesKHR(*swapChain, vks::dynamicDispatchLoader);
//    images = device.getSwapchainImagesKHR(*swapChain);
	imageCount = images.size();

	// Get the swap chain buffers containing the image and imageview
	buffers.resize(imageCount);
	for (uint32_t i = 0; i < imageCount; i++)
	{
		vk::ImageViewCreateInfo colorAttachmentView = {};
		colorAttachmentView.pNext = NULL;
		colorAttachmentView.format = colorFormat;
		colorAttachmentView.components = {
		    vk::ComponentSwizzle::eR,
            vk::ComponentSwizzle::eG,
            vk::ComponentSwizzle::eB,
            vk::ComponentSwizzle::eA,
		};
		colorAttachmentView.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		colorAttachmentView.subresourceRange.baseMipLevel = 0;
		colorAttachmentView.subresourceRange.levelCount = 1;
		colorAttachmentView.subresourceRange.baseArrayLayer = 0;
		colorAttachmentView.subresourceRange.layerCount = 1;
		colorAttachmentView.viewType = vk::ImageViewType::e2D;
		colorAttachmentView.flags = {};

		buffers[i].image = images[i];

		colorAttachmentView.image = buffers[i].image;

		buffers[i].view = device.createImageViewUnique(colorAttachmentView);
	}
}

/** 
* Acquires the next image in the swap chain
*
* @param presentCompleteSemaphore (Optional) Semaphore that is signaled when the image is ready for use
* @param imageIndex Pointer to the image index that will be increased if the next image could be acquired
*
* @note The function will always wait until the next image has been acquired by setting timeout to UINT64_MAX
*
* @return vk::Result of the image acquisition
*/
vk::Result VulkanSwapChain::acquireNextImage(vk::Semaphore presentCompleteSemaphore, uint32_t *imageIndex)
{
	// By setting timeout to UINT64_MAX we will always wait until the next image has been acquired or an actual error is thrown
	// With that we don't have to handle VK_NOT_READY
	auto result = device.acquireNextImageKHR(*swapChain, UINT64_MAX, presentCompleteSemaphore, {}, vks::dynamicDispatchLoader);
//	auto result = device.acquireNextImageKHR(*swapChain, UINT64_MAX, presentCompleteSemaphore, {});
	*imageIndex = result.value;
	return result.result;
}

/**
* Queue an image for presentation
*
* @param queue Presentation queue for presenting the image
* @param imageIndex Index of the swapchain image to queue for presentation
* @param waitSemaphore (Optional) Semaphore that is waited on before the image is presented (only used if != VK_NULL_HANDLE)
*
* @return vk::Result of the queue presentation
*/
vk::Result VulkanSwapChain::queuePresent(vk::Queue queue, uint32_t imageIndex, vk::Semaphore waitSemaphore)
{
	vk::PresentInfoKHR presentInfo = {};
	presentInfo.pNext = NULL;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &*swapChain;
	presentInfo.pImageIndices = &imageIndex;
	// Check if a wait semaphore has been specified to wait for before presenting the image
	if (waitSemaphore)
	{
		presentInfo.pWaitSemaphores = &waitSemaphore;
		presentInfo.waitSemaphoreCount = 1;
	}
	return queue.presentKHR(presentInfo);
}


/**
* Destroy and free Vulkan resources used for the swapchain
*/
void VulkanSwapChain::cleanup()
{
	if (swapChain)
	{
		for (uint32_t i = 0; i < imageCount; i++)
		{
			buffers[i].view.reset();
		}
	}
	if (surface)
	{
		swapChain.reset();
		surface.reset();
	}
}
