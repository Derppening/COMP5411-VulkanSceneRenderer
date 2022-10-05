/*
* Class wrapping access to the swap chain
* 
* A swap chain is a collection of framebuffers used for rendering and presentation to the windowing system
*
* Copyright (C) 2016-2017 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <stdlib.h>
#include <string>
#include <assert.h>
#include <stdio.h>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>
#include "VulkanTools.h"

typedef struct _SwapChainBuffers {
	vk::Image image;
	vk::UniqueImageView view;
} SwapChainBuffer;

class VulkanSwapChain
{
private: 
	vk::Instance instance;
	vk::Device device;
	vk::PhysicalDevice physicalDevice;
	vk::UniqueSurfaceKHR surface;
public:
	vk::Format colorFormat;
	vk::ColorSpaceKHR colorSpace;
	vk::UniqueHandle<vk::SwapchainKHR, vk::DispatchLoaderDynamic> swapChain;
//    vk::UniqueSwapchainKHR swapChain;
	uint32_t imageCount;
	std::vector<vk::Image> images;
	std::vector<SwapChainBuffer> buffers;
	uint32_t queueNodeIndex = UINT32_MAX;

	void initSurface(GLFWwindow* window);

	void connect(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device);
	void create(uint32_t* width, uint32_t* height, bool vsync = false, bool fullscreen = false);
	vk::Result acquireNextImage(vk::Semaphore presentCompleteSemaphore, uint32_t* imageIndex);
	vk::Result queuePresent(vk::Queue queue, uint32_t imageIndex, vk::Semaphore waitSemaphore = {});
	void cleanup();
};
