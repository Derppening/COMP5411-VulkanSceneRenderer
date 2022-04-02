/*
* Vulkan Example base class
*
* Copyright (C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"

#include <fmt/format.h>

std::vector<const char*> VulkanExampleBase::args;

void VulkanExampleBase::createInstance(bool enableValidation)
{
	this->settings.validation = enableValidation;

	// Validation can also be forced via a define
#if defined(_VALIDATION)
	this->settings.validation = true;
#endif

	vk::ApplicationInfo appInfo = {};
	appInfo.pApplicationName = name.c_str();
	appInfo.pEngineName = name.c_str();
	appInfo.apiVersion = apiVersion;

	std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

	// Enable surface extensions depending on os
    {
        uint32_t glfwExtensionCount;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        instanceExtensions.insert(instanceExtensions.end(), glfwExtensions, glfwExtensions + glfwExtensionCount);
    }

	// Get extensions supported by the instance and store for later use
    std::vector<vk::ExtensionProperties> extensions = vk::enumerateInstanceExtensionProperties();
	uint32_t extCount = extensions.size();
	if (extCount > 0)
	{
        for (vk::ExtensionProperties extension : extensions)
        {
            supportedInstanceExtensions.push_back(extension.extensionName);
        }
	}

	// Enabled requested instance extensions
	if (enabledInstanceExtensions.size() > 0)
	{
		for (const char * enabledExtension : enabledInstanceExtensions)
		{
			// Output message if requested extension is not available
			if (std::find(supportedInstanceExtensions.begin(), supportedInstanceExtensions.end(), enabledExtension) == supportedInstanceExtensions.end())
			{
				std::cerr << "Enabled instance extension \"" << enabledExtension << "\" is not present at instance level\n";
			}
			instanceExtensions.push_back(enabledExtension);
		}
	}

	vk::InstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.pNext = NULL;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	if (instanceExtensions.size() > 0)
	{
		if (settings.validation)
		{
			instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
		instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
	}

    // The VK_LAYER_KHRONOS_validation contains all current validation functionality.
    // Note that on Android this layer requires at least NDK r20
    const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
    if (settings.validation)
    {
		// Check if this layer is available at instance level
		std::vector<vk::LayerProperties> instanceLayerProperties = vk::enumerateInstanceLayerProperties();
        uint32_t instanceLayerCount = instanceLayerProperties.size();
		bool validationLayerPresent = false;
		for (vk::LayerProperties layer : instanceLayerProperties) {
			if (strcmp(layer.layerName, validationLayerName) == 0) {
				validationLayerPresent = true;
				break;
			}
		}
		if (validationLayerPresent) {
			instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
			instanceCreateInfo.enabledLayerCount = 1;
		} else {
			std::cerr << "Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled";
		}
	}

	instance = vk::createInstanceUnique(instanceCreateInfo);
}

void VulkanExampleBase::renderFrame()
{
	VulkanExampleBase::prepareFrame();
	if (resized) {
	    resized = false;
	    return;
	}
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &*drawCmdBuffers[currentBuffer];
	queue.submit({submitInfo}, {});
	VulkanExampleBase::submitFrame();
}

std::string VulkanExampleBase::getWindowTitle()
{
	std::string device(deviceProperties.deviceName);
	std::string windowTitle;
	windowTitle = title + " - " + device;
	if (!settings.overlay) {
		windowTitle += " - " + std::to_string(frameCounter) + " fps";
	}
	return windowTitle;
}

void VulkanExampleBase::createCommandBuffers()
{
	// Create one command buffer for each swap chain image and reuse for rendering
	drawCmdBuffers.resize(swapChain.imageCount);

	vk::CommandBufferAllocateInfo cmdBufAllocateInfo =
		vks::initializers::commandBufferAllocateInfo(
			*cmdPool,
			vk::CommandBufferLevel::ePrimary,
			static_cast<uint32_t>(drawCmdBuffers.size()));

	drawCmdBuffers = device.allocateCommandBuffersUnique(cmdBufAllocateInfo);
}

void VulkanExampleBase::destroyCommandBuffers()
{
	drawCmdBuffers.clear();
}

std::string VulkanExampleBase::getShadersPath() const
{
	return getAssetPath() + "shaders/";
}

void VulkanExampleBase::createPipelineCache()
{
	vk::PipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCache = device.createPipelineCacheUnique(pipelineCacheCreateInfo);
}

void VulkanExampleBase::prepare()
{
	if (vulkanDevice->enableDebugMarkers) {
		vks::debugmarker::setup(device);
	}
	initSwapchain();
	createCommandPool();
	setupSwapChain();
	createCommandBuffers();
	createSynchronizationPrimitives();
	setupDepthStencil();
	setupRenderPass();
	createPipelineCache();
	setupFrameBuffer();
	if (settings.overlay) {
		UIOverlay.device = vulkanDevice.get();
		UIOverlay.queue = queue;
		UIOverlay.shaders = {
			loadShader(getShadersPath() + "base/uioverlay.vert.spv", vk::ShaderStageFlagBits::eVertex),
			loadShader(getShadersPath() + "base/uioverlay.frag.spv", vk::ShaderStageFlagBits::eFragment),
		};
		UIOverlay.prepareResources();
		UIOverlay.preparePipeline(*pipelineCache, *renderPass);
	}
}

vk::PipelineShaderStageCreateInfo VulkanExampleBase::loadShader(std::string fileName, vk::ShaderStageFlagBits stage)
{
	auto shader_module = vks::tools::loadShader(fileName.c_str(), device);

    vk::PipelineShaderStageCreateInfo shaderStage = {};
	shaderStage.stage = stage;
	shaderStage.module = *shader_module;
	shaderStage.pName = "main";
	assert(shaderStage.module);
	shaderModules.emplace_back(std::move(shader_module));
	return shaderStage;
}

void VulkanExampleBase::nextFrame()
{
	auto tStart = std::chrono::high_resolution_clock::now();
	if (viewUpdated)
	{
		viewUpdated = false;
		viewChanged();
	}

	render();
	frameCounter++;
	auto tEnd = std::chrono::high_resolution_clock::now();
	auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
	frameTimer = (float)tDiff / 1000.0f;
	camera.update(frameTimer);
	if (camera.moving())
	{
		viewUpdated = true;
	}
	// Convert to clamped timer value
	if (!paused)
	{
		timer += timerSpeed * frameTimer;
		if (timer > 1.0)
		{
			timer -= 1.0f;
		}
	}
	float fpsTimer = (float)(std::chrono::duration<double, std::milli>(tEnd - lastTimestamp).count());
	if (fpsTimer > 1000.0f)
	{
		lastFPS = static_cast<uint32_t>((float)frameCounter * (1000.0f / fpsTimer));
		frameCounter = 0;
		lastTimestamp = tEnd;
	}
	// TODO: Cap UI overlay update rates
	updateOverlay();
}

void VulkanExampleBase::renderLoop()
{

	destWidth = width;
	destHeight = height;
	lastTimestamp = std::chrono::high_resolution_clock::now();
	while (!glfwWindowShouldClose(window)) {
        auto tStart = std::chrono::high_resolution_clock::now();
        if (viewUpdated)
        {
            viewUpdated = false;
            viewChanged();
        }
        processInput();
        render();
        frameCounter++;
        auto tEnd = std::chrono::high_resolution_clock::now();
        auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
        frameTimer = tDiff / 1000.0f;
        camera.update(frameTimer);
        if (camera.moving())
        {
            viewUpdated = true;
        }
        // Convert to clamped timer value
        if (!paused)
        {
            timer += timerSpeed * frameTimer;
            if (timer > 1.0)
            {
                timer -= 1.0f;
            }
        }
        float fpsTimer = std::chrono::duration<double, std::milli>(tEnd - lastTimestamp).count();
        if (fpsTimer > 1000.0f)
        {
            if (!settings.overlay)
            {
                std::string windowTitle = getWindowTitle();
                glfwSetWindowTitle(window, windowTitle.c_str());
            }
            lastFPS = (float)frameCounter * (1000.0f / fpsTimer);
            frameCounter = 0;
            lastTimestamp = tEnd;
        }
        updateOverlay();
        glfwPollEvents();
    }
	// Flush device to make sure all resources can be freed
	if (device) {
	    device.waitIdle();
	}
}

void VulkanExampleBase::updateOverlay()
{
	if (!settings.overlay)
		return;

	ImGuiIO& io = ImGui::GetIO();

	io.DisplaySize = ImVec2((float)width, (float)height);
	io.DeltaTime = frameTimer;

	io.MousePos = ImVec2(mousePos.x, mousePos.y);
	io.MouseDown[0] = mouseButtons.left;
	io.MouseDown[1] = mouseButtons.right;

	ImGui::NewFrame();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::SetNextWindowPos(ImVec2(10, 10));
	ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);
	ImGui::Begin("Vulkan Example", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
	ImGui::TextUnformatted(title.c_str());
	ImGui::TextUnformatted(deviceProperties.deviceName);
	ImGui::Text("%.2f ms/frame (%.1d fps)", (1000.0f / lastFPS), lastFPS);

	ImGui::PushItemWidth(110.0f * UIOverlay.scale);
	OnUpdateUIOverlay(&UIOverlay);
	ImGui::PopItemWidth();

	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::Render();

	if (UIOverlay.update() || UIOverlay.updated) {
		buildCommandBuffers();
		UIOverlay.updated = false;
	}

}

void VulkanExampleBase::drawUI(const vk::CommandBuffer commandBuffer)
{
	if (settings.overlay) {
		const vk::Viewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
		const vk::Rect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
		commandBuffer.setViewport(0, {viewport});
		commandBuffer.setScissor(0, {scissor});

		UIOverlay.draw(commandBuffer);
	}
}

void VulkanExampleBase::prepareFrame()
{
    try {
        // Acquire the next image from the swap chain
        vk::Result result = swapChain.acquireNextImage(*semaphores.presentComplete, &currentBuffer);
        // Recreate the swapchain if it's no longer compatible with the surface (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
        if (result == vk::Result::eSuboptimalKHR) {
            windowResize();
        } else if (result != vk::Result::eSuccess) {
            VK_CHECK_RESULT(result);
        }
    } catch (const vk::OutOfDateKHRError&) {
      windowResize();
    }
}

void VulkanExampleBase::submitFrame()
{
    try {
        vk::Result result = swapChain.queuePresent(queue, currentBuffer, *semaphores.renderComplete);
        if (!((result == vk::Result::eSuccess) || (result == vk::Result::eSuboptimalKHR))) {
            VK_CHECK_RESULT(result);
        }
    } catch (const vk::OutOfDateKHRError&) {
      // Swap chain is no longer compatible with the surface and needs to be recreated
      windowResize();
      return;
    }
    queue.waitIdle();
}

VulkanExampleBase::VulkanExampleBase(bool enableValidation)
{
	// Check for a valid asset path
	struct stat info;
	if (stat(getAssetPath().c_str(), &info) != 0)
	{
		std::cerr << "Error: Could not find asset path in " << getAssetPath() << "\n";
		exit(-1);
	}

	settings.validation = enableValidation;

	// Command line arguments
	commandLineParser.parse(args);
	if (commandLineParser.isSet("help")) {
		commandLineParser.printHelp();
		std::cin.get();
		std::exit(0);
	}
	if (commandLineParser.isSet("validation")) {
		settings.validation = true;
	}
	if (commandLineParser.isSet("vsync")) {
		settings.vsync = true;
	}
	if (commandLineParser.isSet("height")) {
		height = commandLineParser.getValueAsInt("height", width);
	}
	if (commandLineParser.isSet("width")) {
		width = commandLineParser.getValueAsInt("width", width);
	}
	if (commandLineParser.isSet("fullscreen")) {
		settings.fullscreen = true;
	}
}

VulkanExampleBase::~VulkanExampleBase()
{
	// Clean up Vulkan resources
	swapChain.cleanup();
	if (descriptorPool)
	{
		descriptorPool.reset();
	}
	destroyCommandBuffers();
    if (renderPass)
    {
        renderPass.reset();
    }
	frameBuffers.clear();

	shaderModules.clear();
	depthStencil.view.reset();
	depthStencil.image.reset();
	depthStencil.mem.reset();

	pipelineCache.reset();

	cmdPool.reset();

	semaphores.presentComplete.reset();
	semaphores.renderComplete.reset();
	waitFences.clear();

	if (settings.overlay) {
		UIOverlay.freeResources();
	}

	vulkanDevice.reset();

	if (settings.validation)
	{
		vks::debug::freeDebugCallback();
	}

	instance.reset();

	glfwDestroyWindow(window);
	glfwTerminate();
}

bool VulkanExampleBase::initVulkan()
{
	// Vulkan instance
	try {
        createInstance(settings.validation);
    } catch (const vk::SystemError& err) {
        vks::tools::exitFatal("Could not create Vulkan instance : \n" + err.code().message(), err.code().value());
        return false;
    }
	vks::dynamicDispatchLoader.init(*instance);

	// If requested, we enable the default validation layers for debugging
	if (settings.validation)
	{
		// The report flags determine what type of messages for the layers will be displayed
		// For validating (debugging) an application the error and warning bits should suffice
		vk::DebugReportFlagsEXT debugReportFlags = vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eWarning;
		// Additional flags include performance info, loader and layer debug messages, etc.
		vks::debug::setupDebugging(*instance, debugReportFlags, {});
	}

	// Physical device
	uint32_t gpuCount = 0;
	// Enumerate devices
    std::vector<vk::PhysicalDevice> physicalDevices;
	try {
        physicalDevices = instance->enumeratePhysicalDevices();
        gpuCount = physicalDevices.size();
    } catch (const vk::SystemError& err) {
        vks::tools::exitFatal("Could not enumerate physical devices : \n" + err.code().message(), err.code().value());
        return false;
	}

    if (gpuCount == 0) {
		vks::tools::exitFatal("No device with Vulkan support found", -1);
		return false;
	}

	// GPU selection

	// Select physical device to be used for the Vulkan example
	// Defaults to the first device unless specified by command line
	std::uint32_t selectedDevice = 0;

	// GPU selection via command line argument
	if (commandLineParser.isSet("gpuselection")) {
		std::uint32_t index = commandLineParser.getValueAsInt("gpuselection", 0);
		if (index > gpuCount - 1) {
			std::cerr << "Selected device index " << index << " is out of range, reverting to device 0 (use -listgpus to show available Vulkan devices)" << "\n";
		} else {
			selectedDevice = index;
		}
	}
	if (commandLineParser.isSet("gpulist")) {
		std::cout << "Available Vulkan devices" << "\n";
		for (std::uint32_t i = 0; i < gpuCount; i++) {
            vk::PhysicalDeviceProperties deviceProperties = physicalDevices[i].getProperties();
			std::cout << "Device [" << i << "] : " << deviceProperties.deviceName << std::endl;
			std::cout << " Type: " << vks::tools::physicalDeviceTypeString(deviceProperties.deviceType) << "\n";
			std::cout << " API: " << (deviceProperties.apiVersion >> 22) << "." << ((deviceProperties.apiVersion >> 12) & 0x3ff) << "." << (deviceProperties.apiVersion & 0xfff) << "\n";
		}
	}

	physicalDevice = physicalDevices[selectedDevice];

	// Store properties (including limits), features and memory properties of the physical device (so that examples can check against them)
	deviceProperties = physicalDevice.getProperties();
	deviceFeatures = physicalDevice.getFeatures();
	deviceMemoryProperties = physicalDevice.getMemoryProperties();

	// Derived examples can override this to set actual features (based on above readings) to enable for logical device creation
	getEnabledFeatures();

	// Vulkan device creation
	// This is handled by a separate class that gets a logical device representation
	// and encapsulates functions related to a device
	vulkanDevice = std::make_unique<vks::VulkanDevice>(physicalDevice);
	vk::Result res = vulkanDevice->createLogicalDevice(enabledFeatures, enabledDeviceExtensions, deviceCreatepNextChain);
	if (res != vk::Result::eSuccess) {
		vks::tools::exitFatal("Could not create Vulkan device: \n" + vks::tools::errorString(vk::Result(res)), res);
		return false;
	}
	device = *vulkanDevice->logicalDevice;
	vks::dynamicDispatchLoader.init(device);

	// Get a graphics queue from the device
	queue = device.getQueue(vulkanDevice->queueFamilyIndices.graphics, 0);

	// Find a suitable depth format
	std::optional validDepthFormat = vks::tools::getSupportedDepthFormat(physicalDevice);
	assert(validDepthFormat.has_value());
	depthFormat = *validDepthFormat;

	swapChain.connect(*instance, physicalDevice, device);

	// Create synchronization objects
	vk::SemaphoreCreateInfo semaphoreCreateInfo = vks::initializers::semaphoreCreateInfo();
	// Create a semaphore used to synchronize image presentation
	// Ensures that the image is displayed before we start submitting new commands to the queue
	semaphores.presentComplete = device.createSemaphoreUnique(semaphoreCreateInfo);
	// Create a semaphore used to synchronize command submission
	// Ensures that the image is not presented until all commands have been submitted and executed
	semaphores.renderComplete = device.createSemaphoreUnique(semaphoreCreateInfo);

	// Set up submit info structure
	// Semaphores will stay the same during application lifetime
	// Command buffer submission info is set by each example
	submitInfo = vks::initializers::submitInfo();
	submitInfo.pWaitDstStageMask = &submitPipelineStages;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &*semaphores.presentComplete;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &*semaphores.renderComplete;

	return true;
}

GLFWwindow* VulkanExampleBase::setupWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    if (settings.fullscreen) {
      auto primaryMonitor = glfwGetPrimaryMonitor();
      const auto mode = glfwGetVideoMode(primaryMonitor);
      width = mode->width;
      height = mode->height;
      window = glfwCreateWindow(width, height, title.c_str(), glfwGetPrimaryMonitor(), nullptr);
    } else {
      window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    }
    glfwSetWindowUserPointer(window, this);
    glfwSetCursorPosCallback(window, cursorCallback);
    glfwSetMouseButtonCallback(window, mouseBtnCallback);
    glfwSetKeyCallback(window, keyCallback);
    return window;
}

void VulkanExampleBase::processInput() {
  camera.keys.up = (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS);
  camera.keys.down = (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS);
  camera.keys.left = (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS);
  camera.keys.right = (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS);

  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
}

void VulkanExampleBase::cursorCallback(GLFWwindow* window, double xpos, double ypos) {
  auto app = reinterpret_cast<VulkanExampleBase*>(glfwGetWindowUserPointer(window));
  app->handleMouseMove(xpos, ypos);
}

void VulkanExampleBase::mouseBtnCallback(GLFWwindow* window, int button, int action, int mods) {
  auto app = reinterpret_cast<VulkanExampleBase*>(glfwGetWindowUserPointer(window));
  switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT:
      app->mouseButtons.left = (action == GLFW_PRESS);
      break;
    case GLFW_MOUSE_BUTTON_MIDDLE:
      app->mouseButtons.middle = (action == GLFW_PRESS);
      break;
    case GLFW_MOUSE_BUTTON_RIGHT:
      app->mouseButtons.right = (action == GLFW_PRESS);
      break;
    default:
      // no-op
      break;
  }
}

void VulkanExampleBase::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  auto app = reinterpret_cast<VulkanExampleBase*>(glfwGetWindowUserPointer(window));
  switch (key) {
    case GLFW_KEY_P:
      if (action == GLFW_PRESS) {
        app->paused = !app->paused;
      }
      break;
    case GLFW_KEY_F1:
      if (action == GLFW_PRESS && app->settings.overlay) {
        app->settings.overlay = !app->settings.overlay;
      }
      break;
    default:
      // no-op
      break;
  }

  app->keyPressed(key);
}

void VulkanExampleBase::windowResizeCallback(GLFWwindow* window, int width, int height) {
  auto app = reinterpret_cast<VulkanExampleBase*>(glfwGetWindowUserPointer(window));
  if ((app->prepared) && (width != app->width || height != app->height)) {
    app->destWidth = width;
    app->destHeight = height;
    if ((app->destWidth > 0) && (app->destHeight > 0)) {
      app->windowResize();
    }
  }
}

void VulkanExampleBase::viewChanged() {}

void VulkanExampleBase::keyPressed(uint32_t) {}

void VulkanExampleBase::mouseMoved(double x, double y, bool & handled) {}

void VulkanExampleBase::buildCommandBuffers() {}

void VulkanExampleBase::createSynchronizationPrimitives()
{
	// Wait fences to sync command buffer access
	vk::FenceCreateInfo fenceCreateInfo = vks::initializers::fenceCreateInfo(vk::FenceCreateFlagBits::eSignaled);
	waitFences.resize(drawCmdBuffers.size());
	for (auto& fence : waitFences) {
	    fence = device.createFenceUnique(fenceCreateInfo);
	}
}

void VulkanExampleBase::createCommandPool()
{
	vk::CommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.queueFamilyIndex = swapChain.queueNodeIndex;
	cmdPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
	cmdPool = device.createCommandPoolUnique(cmdPoolInfo);
}

void VulkanExampleBase::setupDepthStencil()
{
	vk::ImageCreateInfo imageCI{};
	imageCI.imageType = vk::ImageType::e2D;
	imageCI.format = depthFormat;
	imageCI.extent = vk::Extent3D{ width, height, 1 };
	imageCI.mipLevels = 1;
	imageCI.arrayLayers = 1;
	imageCI.samples = vk::SampleCountFlagBits::e1;
	imageCI.tiling = vk::ImageTiling::eOptimal;
	imageCI.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;

    depthStencil.image = device.createImageUnique(imageCI);
	vk::MemoryRequirements memReqs = device.getImageMemoryRequirements(*depthStencil.image);

	vk::MemoryAllocateInfo memAllloc{};
	memAllloc.allocationSize = memReqs.size;
	memAllloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
	depthStencil.mem = device.allocateMemoryUnique(memAllloc);
	device.bindImageMemory(*depthStencil.image, *depthStencil.mem, 0);

	vk::ImageViewCreateInfo imageViewCI{};
	imageViewCI.viewType = vk::ImageViewType::e2D;
	imageViewCI.image = *depthStencil.image;
	imageViewCI.format = depthFormat;
	imageViewCI.subresourceRange.baseMipLevel = 0;
	imageViewCI.subresourceRange.levelCount = 1;
	imageViewCI.subresourceRange.baseArrayLayer = 0;
	imageViewCI.subresourceRange.layerCount = 1;
	imageViewCI.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
	// Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT
	if (depthFormat >= vk::Format::eD16UnormS8Uint) {
		imageViewCI.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
	}
	depthStencil.view = device.createImageViewUnique(imageViewCI);
}

void VulkanExampleBase::setupFrameBuffer()
{
	std::array<vk::ImageView, 2> attachments;

	// Depth/Stencil attachment is the same for all frame buffers
	attachments[1] = *depthStencil.view;

	vk::FramebufferCreateInfo frameBufferCreateInfo = {};
	frameBufferCreateInfo.pNext = NULL;
	frameBufferCreateInfo.renderPass = *renderPass;
	frameBufferCreateInfo.attachmentCount = attachments.size();
	frameBufferCreateInfo.pAttachments = attachments.data();
	frameBufferCreateInfo.width = width;
	frameBufferCreateInfo.height = height;
	frameBufferCreateInfo.layers = 1;

	// Create frame buffers for every swap chain image
	frameBuffers.resize(swapChain.imageCount);
	for (uint32_t i = 0; i < frameBuffers.size(); i++)
	{
		attachments[0] = *swapChain.buffers[i].view;
		frameBuffers[i] = device.createFramebufferUnique(frameBufferCreateInfo);
	}
}

void VulkanExampleBase::setupRenderPass()
{
	std::array<vk::AttachmentDescription, 2> attachments = {};
	// Color attachment
	attachments[0].format = swapChain.colorFormat;
	attachments[0].samples = vk::SampleCountFlagBits::e1;
	attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
	attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
	attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	attachments[0].initialLayout = vk::ImageLayout::eUndefined;
	attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;
	// Depth attachment
	attachments[1].format = depthFormat;
	attachments[1].samples = vk::SampleCountFlagBits::e1;
	attachments[1].loadOp = vk::AttachmentLoadOp::eClear;
	attachments[1].storeOp = vk::AttachmentStoreOp::eStore;
	attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eClear;
	attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	attachments[1].initialLayout = vk::ImageLayout::eUndefined;
	attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	vk::AttachmentReference colorReference = {};
	colorReference.attachment = 0;
	colorReference.layout = vk::ImageLayout::eColorAttachmentOptimal;

	vk::AttachmentReference depthReference = {};
	depthReference.attachment = 1;
	depthReference.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	vk::SubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorReference;
	subpassDescription.pDepthStencilAttachment = &depthReference;
	subpassDescription.inputAttachmentCount = 0;
	subpassDescription.pInputAttachments = nullptr;
	subpassDescription.preserveAttachmentCount = 0;
	subpassDescription.pPreserveAttachments = nullptr;
	subpassDescription.pResolveAttachments = nullptr;

	// Subpass dependencies for layout transitions
	std::array<vk::SubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
	dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
	dependencies[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
	dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
	dependencies[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
	dependencies[1].dstAccessMask = vk::AccessFlagBits::eMemoryRead;
	dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

	vk::RenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	renderPass = device.createRenderPassUnique(renderPassInfo);
}

void VulkanExampleBase::getEnabledFeatures() {}

void VulkanExampleBase::windowResize()
{
	if (!prepared)
	{
		return;
	}
	prepared = false;
	resized = true;

	// Ensure all operations on the device have been finished before destroying resources
	device.waitIdle();

	// Recreate swap chain
	width = destWidth;
	height = destHeight;
	setupSwapChain();

	// Recreate the frame buffers
	depthStencil.view.reset();
	depthStencil.image.reset();
	depthStencil.mem.reset();
	setupDepthStencil();
	frameBuffers.clear();
	setupFrameBuffer();

	if ((width > 0.0f) && (height > 0.0f)) {
		if (settings.overlay) {
			UIOverlay.resize(width, height);
		}
	}

	// Command buffers need to be recreated as they may store
	// references to the recreated frame buffer
	destroyCommandBuffers();
	createCommandBuffers();
	buildCommandBuffers();

	device.waitIdle();

	if ((width > 0.0f) && (height > 0.0f)) {
		camera.updateAspectRatio((float)width / (float)height);
	}

	// Notify derived class
	windowResized();
	viewChanged();

	prepared = true;
}

void VulkanExampleBase::handleMouseMove(int32_t x, int32_t y)
{
	int32_t dx = (int32_t)mousePos.x - x;
	int32_t dy = (int32_t)mousePos.y - y;

	bool handled = false;

	if (settings.overlay) {
		ImGuiIO& io = ImGui::GetIO();
		handled = io.WantCaptureMouse;
	}
	mouseMoved((float)x, (float)y, handled);

	if (handled) {
		mousePos = glm::vec2((float)x, (float)y);
		return;
	}

	if (mouseButtons.left) {
		camera.rotate(glm::vec3(dy * camera.rotationSpeed, -dx * camera.rotationSpeed, 0.0f));
		viewUpdated = true;
	}
	if (mouseButtons.right) {
		camera.translate(glm::vec3(-0.0f, 0.0f, dy * .005f));
		viewUpdated = true;
	}
	if (mouseButtons.middle) {
		camera.translate(glm::vec3(-dx * 0.01f, -dy * 0.01f, 0.0f));
		viewUpdated = true;
	}
	mousePos = glm::vec2((float)x, (float)y);
}

void VulkanExampleBase::windowResized() {}

void VulkanExampleBase::initSwapchain()
{
	swapChain.initSurface(window);
}

void VulkanExampleBase::setupSwapChain()
{
	swapChain.create(&width, &height, settings.vsync);
}

void VulkanExampleBase::OnUpdateUIOverlay(vks::UIOverlay *overlay) {}

// Command line argument parser class

CommandLineParser::CommandLineParser()
{
	add("help", { "--help" }, 0, "Show help");
	add("validation", {"-v", "--validation"}, 0, "Enable validation layers");
	add("vsync", {"-vs", "--vsync"}, 0, "Enable V-Sync");
	add("fullscreen", { "-f", "--fullscreen" }, 0, "Start in fullscreen mode");
	add("width", { "-w", "--width" }, 1, "Set window width");
	add("height", { "-h", "--height" }, 1, "Set window height");
	add("gpuselection", { "-g", "--gpu" }, 1, "Select GPU to run on");
	add("gpulist", { "-gl", "--listgpus" }, 0, "Display a list of available Vulkan devices");
}

void CommandLineParser::add(const std::string& name, const std::vector<std::string>& commands, bool hasValue, const std::string& help)
{
	options[name].commands = commands;
	options[name].help = help;
	options[name].set = false;
	options[name].hasValue = hasValue;
	options[name].value = "";
}

void CommandLineParser::printHelp()
{
	std::cout << "Available command line options:\n";
	for (auto option : options) {
		std::cout << " ";
		for (std::size_t i = 0; i < option.second.commands.size(); i++) {
			std::cout << option.second.commands[i];
			if (i < option.second.commands.size() - 1) {
				std::cout << ", ";
			}
		}
		std::cout << ": " << option.second.help << "\n";
	}
	std::cout << "Press any key to close...";
}

void CommandLineParser::parse(const std::vector<const char*>& arguments)
{
	bool printHelp = false;
	// Known arguments
	for (auto& option : options) {
		for (const auto& command : option.second.commands) {
			for (std::size_t i = 0; i < arguments.size(); i++) {
				if (std::strcmp(arguments[i], command.c_str()) == 0) {
					option.second.set = true;
					// Get value
					if (option.second.hasValue) {
						if (arguments.size() > i + 1) {
							option.second.value = arguments[i + 1];
						}
						if (option.second.value == "") {
							printHelp = true;
							break;
						}
					}
				}
			}
		}
	}
	// Print help for unknown arguments or missing argument values
	if (printHelp) {
		options["help"].set = true;
	}
}

bool CommandLineParser::isSet(const std::string& name)
{
	return ((options.find(name) != options.end()) && options[name].set);
}

std::string CommandLineParser::getValueAsString(const std::string& name, const std::string& defaultValue)
{
	assert(options.find(name) != options.end());
	std::string value = options[name].value;
	return (value != "") ? value : defaultValue;
}

std::int32_t CommandLineParser::getValueAsInt(const std::string& name, std::int32_t defaultValue)
{
	assert(options.find(name) != options.end());
	std::string value = options[name].value;
	if (value != "") {
		char* numConvPtr;
		std::int32_t intVal = std::strtol(value.c_str(), &numConvPtr, 10);
		return (intVal > 0) ? intVal : defaultValue;
	} else {
		return defaultValue;
	}
	return std::int32_t();
}
