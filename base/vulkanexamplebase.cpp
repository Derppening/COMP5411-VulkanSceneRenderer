/*
* Vulkan Example base class
*
* Copyright (C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"

#include <fmt/format.h>

#if (defined(VK_USE_PLATFORM_MACOS_MVK) && defined(VK_EXAMPLE_XCODE_GENERATED))
#include <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>
#include <QuartzCore/CAMetalLayer.h>
#include <CoreVideo/CVDisplayLink.h>
#endif

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
#if defined(_WIN32)
	instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(_DIRECT2DISPLAY)
	instanceExtensions.push_back(VK_KHR_DISPLAY_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
	instanceExtensions.push_back(VK_EXT_DIRECTFB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
	instanceExtensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	instanceExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
	instanceExtensions.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
#else
    {
        uint32_t glfwExtensionCount;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        instanceExtensions.insert(instanceExtensions.end(), glfwExtensions, glfwExtensions + glfwExtensionCount);
    }
#endif

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
	if (settings.validation)
	{
		// The VK_LAYER_KHRONOS_validation contains all current validation functionality.
		// Note that on Android this layer requires at least NDK r20
		const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
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
#if defined(_WIN32)
		if (!settings.overlay)	{
			std::string windowTitle = getWindowTitle();
			SetWindowText(window, windowTitle.c_str());
		}
#endif
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
#if defined(_WIN32)
	MSG msg;
	bool quitMessageReceived = false;
	while (!quitMessageReceived) {
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT) {
				quitMessageReceived = true;
				break;
			}
		}
		if (prepared && !IsIconic(window)) {
			nextFrame();
		}
	}
#elif defined(_DIRECT2DISPLAY)
	while (!quit)
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
			lastFPS = (float)frameCounter * (1000.0f / fpsTimer);
			frameCounter = 0;
			lastTimestamp = tEnd;
		}
		updateOverlay();
	}
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
	while (!quit)
	{
		auto tStart = std::chrono::high_resolution_clock::now();
		if (viewUpdated)
		{
			viewUpdated = false;
			viewChanged();
		}
		DFBWindowEvent event;
		while (!event_buffer->GetEvent(event_buffer, DFB_EVENT(&event)))
		{
			handleEvent(&event);
		}
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
			lastFPS = (float)frameCounter * (1000.0f / fpsTimer);
			frameCounter = 0;
			lastTimestamp = tEnd;
		}
		updateOverlay();
	}
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
	while (!quit)
	{
		auto tStart = std::chrono::high_resolution_clock::now();
		if (viewUpdated)
		{
			viewUpdated = false;
			viewChanged();
		}

		while (!configured)
			wl_display_dispatch(display);
		while (wl_display_prepare_read(display) != 0)
			wl_display_dispatch_pending(display);
		wl_display_flush(display);
		wl_display_read_events(display);
		wl_display_dispatch_pending(display);

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
				xdg_toplevel_set_title(xdg_toplevel, windowTitle.c_str());
			}
			lastFPS = (float)frameCounter * (1000.0f / fpsTimer);
			frameCounter = 0;
			lastTimestamp = tEnd;
		}
		updateOverlay();
	}
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	xcb_flush(connection);
	while (!quit)
	{
		auto tStart = std::chrono::high_resolution_clock::now();
		if (viewUpdated)
		{
			viewUpdated = false;
			viewChanged();
		}
		xcb_generic_event_t *event;
		while ((event = xcb_poll_for_event(connection)))
		{
			handleEvent(event);
			free(event);
		}
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
				xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
					window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
					windowTitle.size(), windowTitle.c_str());
			}
			lastFPS = (float)frameCounter * (1000.0f / fpsTimer);
			frameCounter = 0;
			lastTimestamp = tEnd;
		}
		updateOverlay();
	}
#elif (defined(VK_USE_PLATFORM_MACOS_MVK) && defined(VK_EXAMPLE_XCODE_GENERATED))
	[NSApp run];
#else
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
#endif
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
#if defined(_WIN32)
		std::string msg = "Could not locate asset path in \"" + getAssetPath() + "\" !";
		MessageBox(NULL, msg.c_str(), "Fatal error", MB_OK | MB_ICONERROR);
#else
		std::cerr << "Error: Could not find asset path in " << getAssetPath() << "\n";
#endif
		exit(-1);
	}

	settings.validation = enableValidation;

	char* numConvPtr;

	// Parse command line arguments
	for (size_t i = 0; i < args.size(); i++)
	{
		if (args[i] == std::string("-validation")) {
			settings.validation = true;
		}
		if (args[i] == std::string("-vsync")) {
			settings.vsync = true;
		}
		if ((args[i] == std::string("-f")) || (args[i] == std::string("--fullscreen"))) {
			settings.fullscreen = true;
		}
		if ((args[i] == std::string("-w")) || (args[i] == std::string("-width"))) {
			uint32_t w = strtol(args[i + 1], &numConvPtr, 10);
			if (numConvPtr != args[i + 1]) { width = w; };
		}
		if ((args[i] == std::string("-h")) || (args[i] == std::string("-height"))) {
			uint32_t h = strtol(args[i + 1], &numConvPtr, 10);
			if (numConvPtr != args[i + 1]) { height = h; };
		}
	}

#if defined(_DIRECT2DISPLAY)

#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
	initWaylandConnection();
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	initxcbConnection();
#endif

#if defined(_WIN32)
	// Enable console if validation is active
	// Debug message callback will output to it
	if (this->settings.validation)
	{
		setupConsole("Vulkan validation output");
	}
	setupDPIAwareness();
#endif
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
	renderPass.reset();
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

#if defined(_DIRECT2DISPLAY)

#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
	if (event_buffer)
		event_buffer->Release(event_buffer);
	if (surface)
		surface->Release(surface);
	if (window)
		window->Release(window);
	if (layer)
		layer->Release(layer);
	if (dfb)
		dfb->Release(dfb);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
	xdg_toplevel_destroy(xdg_toplevel);
	xdg_surface_destroy(xdg_surface);
	wl_surface_destroy(surface);
	if (keyboard)
		wl_keyboard_destroy(keyboard);
	if (pointer)
		wl_pointer_destroy(pointer);
	wl_seat_destroy(seat);
	xdg_wm_base_destroy(shell);
	wl_compositor_destroy(compositor);
	wl_registry_destroy(registry);
	wl_display_disconnect(display);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	xcb_destroy_window(connection, window);
	xcb_disconnect(connection);
#else
	glfwDestroyWindow(window);
	glfwTerminate();
#endif
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

	// GPU selection

	// Select physical device to be used for the Vulkan example
	// Defaults to the first device unless specified by command line
	uint32_t selectedDevice = 0;

	// GPU selection via command line argument
	for (size_t i = 0; i < args.size(); i++)
	{
		// Select GPU
		if ((args[i] == std::string("-g")) || (args[i] == std::string("-gpu")))
		{
			char* endptr;
			uint32_t index = strtol(args[i + 1], &endptr, 10);
			if (endptr != args[i + 1])
			{
				if (index > gpuCount - 1)
				{
					std::cerr << "Selected device index " << index << " is out of range, reverting to device 0 (use -listgpus to show available Vulkan devices)" << "\n";
				}
				else
				{
					std::cout << "Selected Vulkan device " << index << "\n";
					selectedDevice = index;
				}
			};
			break;
		}
		// List available GPUs
		if (args[i] == std::string("-listgpus"))
        {
            uint32_t gpuCount = instance->enumeratePhysicalDevices().size();
			if (gpuCount == 0)
			{
				std::cerr << "No Vulkan devices found!" << "\n";
			}
			else
			{
				// Enumerate devices
				std::cout << "Available Vulkan devices" << "\n";
				std::vector<vk::PhysicalDevice> devices = instance->enumeratePhysicalDevices();
				for (uint32_t j = 0; j < gpuCount; j++) {
					vk::PhysicalDeviceProperties deviceProperties = devices[j].getProperties();
					std::cout << "Device [" << j << "] : " << deviceProperties.deviceName << std::endl;
					std::cout << " Type: " << vks::tools::physicalDeviceTypeString(deviceProperties.deviceType) << "\n";
					std::cout << " API: " << (deviceProperties.apiVersion >> 22) << "." << ((deviceProperties.apiVersion >> 12) & 0x3ff) << "." << (deviceProperties.apiVersion & 0xfff) << "\n";
				}
			}
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

#if defined(_WIN32)
// Win32 : Sets up a console window and redirects standard output to it
void VulkanExampleBase::setupConsole(std::string title)
{
	AllocConsole();
	AttachConsole(GetCurrentProcessId());
	FILE *stream;
	freopen_s(&stream, "CONOUT$", "w+", stdout);
	freopen_s(&stream, "CONOUT$", "w+", stderr);
	SetConsoleTitle(TEXT(title.c_str()));
}

void VulkanExampleBase::setupDPIAwareness()
{
	typedef HRESULT *(__stdcall *SetProcessDpiAwarenessFunc)(PROCESS_DPI_AWARENESS);

	HMODULE shCore = LoadLibraryA("Shcore.dll");
	if (shCore)
	{
		SetProcessDpiAwarenessFunc setProcessDpiAwareness =
			(SetProcessDpiAwarenessFunc)GetProcAddress(shCore, "SetProcessDpiAwareness");

		if (setProcessDpiAwareness != nullptr)
		{
			setProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
		}

		FreeLibrary(shCore);
	}
}

HWND VulkanExampleBase::setupWindow(HINSTANCE hinstance, WNDPROC wndproc)
{
	this->windowInstance = hinstance;

	WNDCLASSEX wndClass;

	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = wndproc;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = hinstance;
	wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wndClass.lpszMenuName = NULL;
	wndClass.lpszClassName = name.c_str();
	wndClass.hIconSm = LoadIcon(NULL, IDI_WINLOGO);

	if (!RegisterClassEx(&wndClass))
	{
		std::cout << "Could not register window class!\n";
		fflush(stdout);
		exit(1);
	}

	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	if (settings.fullscreen)
	{
		if ((width != (uint32_t)screenWidth) && (height != (uint32_t)screenHeight))
		{
			DEVMODE dmScreenSettings;
			memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
			dmScreenSettings.dmSize       = sizeof(dmScreenSettings);
			dmScreenSettings.dmPelsWidth  = width;
			dmScreenSettings.dmPelsHeight = height;
			dmScreenSettings.dmBitsPerPel = 32;
			dmScreenSettings.dmFields     = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
			if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
			{
				if (MessageBox(NULL, "Fullscreen Mode not supported!\n Switch to window mode?", "Error", MB_YESNO | MB_ICONEXCLAMATION) == IDYES)
				{
					settings.fullscreen = false;
				}
				else
				{
					return nullptr;
				}
			}
			screenWidth = width;
			screenHeight = height;
		}

	}

	DWORD dwExStyle;
	DWORD dwStyle;

	if (settings.fullscreen)
	{
		dwExStyle = WS_EX_APPWINDOW;
		dwStyle = WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	}
	else
	{
		dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
		dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	}

	RECT windowRect;
	windowRect.left = 0L;
	windowRect.top = 0L;
	windowRect.right = settings.fullscreen ? (long)screenWidth : (long)width;
	windowRect.bottom = settings.fullscreen ? (long)screenHeight : (long)height;

	AdjustWindowRectEx(&windowRect, dwStyle, FALSE, dwExStyle);

	std::string windowTitle = getWindowTitle();
	window = CreateWindowEx(0,
		name.c_str(),
		windowTitle.c_str(),
		dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
		0,
		0,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		NULL,
		NULL,
		hinstance,
		NULL);

	if (!settings.fullscreen)
	{
		// Center on screen
		uint32_t x = (GetSystemMetrics(SM_CXSCREEN) - windowRect.right) / 2;
		uint32_t y = (GetSystemMetrics(SM_CYSCREEN) - windowRect.bottom) / 2;
		SetWindowPos(window, 0, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
	}

	if (!window)
	{
		printf("Could not create window!\n");
		fflush(stdout);
		return nullptr;
	}

	ShowWindow(window, SW_SHOW);
	SetForegroundWindow(window);
	SetFocus(window);

	return window;
}

void VulkanExampleBase::handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE:
		prepared = false;
		DestroyWindow(hWnd);
		PostQuitMessage(0);
		break;
	case WM_PAINT:
		ValidateRect(window, NULL);
		break;
	case WM_KEYDOWN:
		switch (wParam)
		{
		case KEY_P:
			paused = !paused;
			break;
		case KEY_F1:
			if (settings.overlay) {
				UIOverlay.visible = !UIOverlay.visible;
			}
			break;
		case KEY_ESCAPE:
			PostQuitMessage(0);
			break;
		}

		if (camera.type == Camera::firstperson)
		{
			switch (wParam)
			{
			case KEY_W:
				camera.keys.up = true;
				break;
			case KEY_S:
				camera.keys.down = true;
				break;
			case KEY_A:
				camera.keys.left = true;
				break;
			case KEY_D:
				camera.keys.right = true;
				break;
			}
		}

		keyPressed((uint32_t)wParam);
		break;
	case WM_KEYUP:
		if (camera.type == Camera::firstperson)
		{
			switch (wParam)
			{
			case KEY_W:
				camera.keys.up = false;
				break;
			case KEY_S:
				camera.keys.down = false;
				break;
			case KEY_A:
				camera.keys.left = false;
				break;
			case KEY_D:
				camera.keys.right = false;
				break;
			}
		}
		break;
	case WM_LBUTTONDOWN:
		mousePos = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
		mouseButtons.left = true;
		break;
	case WM_RBUTTONDOWN:
		mousePos = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
		mouseButtons.right = true;
		break;
	case WM_MBUTTONDOWN:
		mousePos = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
		mouseButtons.middle = true;
		break;
	case WM_LBUTTONUP:
		mouseButtons.left = false;
		break;
	case WM_RBUTTONUP:
		mouseButtons.right = false;
		break;
	case WM_MBUTTONUP:
		mouseButtons.middle = false;
		break;
	case WM_MOUSEWHEEL:
	{
		short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
		camera.translate(glm::vec3(0.0f, 0.0f, (float)wheelDelta * 0.005f));
		viewUpdated = true;
		break;
	}
	case WM_MOUSEMOVE:
	{
		handleMouseMove(LOWORD(lParam), HIWORD(lParam));
		break;
	}
	case WM_SIZE:
		if ((prepared) && (wParam != SIZE_MINIMIZED))
		{
			if ((resizing) || ((wParam == SIZE_MAXIMIZED) || (wParam == SIZE_RESTORED)))
			{
				destWidth = LOWORD(lParam);
				destHeight = HIWORD(lParam);
				windowResize();
			}
		}
		break;
	case WM_GETMINMAXINFO:
	{
		LPMINMAXINFO minMaxInfo = (LPMINMAXINFO)lParam;
		minMaxInfo->ptMinTrackSize.x = 64;
		minMaxInfo->ptMinTrackSize.y = 64;
		break;
	}
	case WM_ENTERSIZEMOVE:
		resizing = true;
		break;
	case WM_EXITSIZEMOVE:
		resizing = false;
		break;
	}
}
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
#if defined(VK_EXAMPLE_XCODE_GENERATED)
@interface AppDelegate : NSObject<NSApplicationDelegate>
{
}

@end

@implementation AppDelegate
{
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
	return YES;
}

@end

static CVReturn displayLinkOutputCallback(CVDisplayLinkRef displayLink, const CVTimeStamp *inNow,
	const CVTimeStamp *inOutputTime, CVOptionFlags flagsIn, CVOptionFlags *flagsOut,
	void *displayLinkContext)
{
	@autoreleasepool
	{
		auto vulkanExample = static_cast<VulkanExampleBase*>(displayLinkContext);
			vulkanExample->displayLinkOutputCb();
	}
	return kCVReturnSuccess;
}

@interface View : NSView<NSWindowDelegate>
{
@public
	VulkanExampleBase *vulkanExample;
}

@end

@implementation View
{
	CVDisplayLinkRef displayLink;
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
	self = [super initWithFrame:(frameRect)];
	if (self)
	{
		self.wantsLayer = YES;
		self.layer = [CAMetalLayer layer];
	}
	return self;
}

- (void)viewDidMoveToWindow
{
	CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
	CVDisplayLinkSetOutputCallback(displayLink, &displayLinkOutputCallback, vulkanExample);
	CVDisplayLinkStart(displayLink);
}

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (void)keyDown:(NSEvent*)event
{
	switch (event.keyCode)
	{
		case kVK_ANSI_P:
			vulkanExample->paused = !vulkanExample->paused;
			break;
		case kVK_Escape:
			[NSApp terminate:nil];
			break;
		case kVK_ANSI_W:
			vulkanExample->camera.keys.up = true;
			break;
		case kVK_ANSI_S:
			vulkanExample->camera.keys.down = true;
			break;
		case kVK_ANSI_A:
			vulkanExample->camera.keys.left = true;
			break;
		case kVK_ANSI_D:
			vulkanExample->camera.keys.right = true;
			break;
		default:
			break;
	}
}

- (void)keyUp:(NSEvent*)event
{
	switch (event.keyCode)
	{
		case kVK_ANSI_W:
			vulkanExample->camera.keys.up = false;
			break;
		case kVK_ANSI_S:
			vulkanExample->camera.keys.down = false;
			break;
		case kVK_ANSI_A:
			vulkanExample->camera.keys.left = false;
		break;
			case kVK_ANSI_D:
		vulkanExample->camera.keys.right = false;
			break;
		default:
			break;
	}
}

- (NSPoint)getMouseLocalPoint:(NSEvent*)event
{
	NSPoint location = [event locationInWindow];
	NSPoint point = [self convertPoint:location fromView:nil];
	point.y = self.frame.size.height - point.y;
	return point;
}

- (void)mouseDown:(NSEvent *)event
{
	auto point = [self getMouseLocalPoint:event];
	vulkanExample->mousePos = glm::vec2(point.x, point.y);
	vulkanExample->mouseButtons.left = true;
}

- (void)mouseUp:(NSEvent *)event
{
	auto point = [self getMouseLocalPoint:event];
	vulkanExample->mousePos = glm::vec2(point.x, point.y);
	vulkanExample->mouseButtons.left = false;
}

- (void)otherMouseDown:(NSEvent *)event
{
	vulkanExample->mouseButtons.right = true;
}

- (void)otherMouseUp:(NSEvent *)event
{
	vulkanExample->mouseButtons.right = false;
}

- (void)mouseDragged:(NSEvent *)event
{
	auto point = [self getMouseLocalPoint:event];
	vulkanExample->mouseDragged(point.x, point.y);
}

- (void)mouseMoved:(NSEvent *)event
{
	auto point = [self getMouseLocalPoint:event];
	vulkanExample->mouseDragged(point.x, point.y);
}

- (void)scrollWheel:(NSEvent *)event
{
	short wheelDelta = [event deltaY];
	vulkanExample->camera.translate(glm::vec3(0.0f, 0.0f,
		-(float)wheelDelta * 0.05f * vulkanExample->camera.movementSpeed));
}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize
{
	CVDisplayLinkStop(displayLink);
	vulkanExample->windowWillResize(frameSize.width, frameSize.height);
	return frameSize;
}

- (void)windowDidResize:(NSNotification *)notification
{
	vulkanExample->windowDidResize();
	CVDisplayLinkStart(displayLink);
}

- (BOOL)windowShouldClose:(NSWindow *)sender
{
	return TRUE;
}

- (void)windowWillClose:(NSNotification *)notification
{
	CVDisplayLinkStop(displayLink);
}

@end
#endif

void* VulkanExampleBase::setupWindow(void* view)
{
#if defined(VK_EXAMPLE_XCODE_GENERATED)
	NSApp = [NSApplication sharedApplication];
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
	[NSApp setDelegate:[AppDelegate new]];

	const auto kContentRect = NSMakeRect(0.0f, 0.0f, width, height);
	const auto kWindowStyle = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable;

	auto window = [[NSWindow alloc] initWithContentRect:kContentRect
											  styleMask:kWindowStyle
												backing:NSBackingStoreBuffered
												  defer:NO];
	[window setTitle:@(title.c_str())];
	[window setAcceptsMouseMovedEvents:YES];
	[window center];
	[window makeKeyAndOrderFront:nil];

	auto nsView = [[View alloc] initWithFrame:kContentRect];
	nsView->vulkanExample = this;
	[window setDelegate:nsView];
	[window setContentView:nsView];
	this->view = (__bridge void*)nsView;
#else
	this->view = view;
#endif
	return view;
}

void VulkanExampleBase::displayLinkOutputCb()
{
	if (prepared)
		nextFrame();
}

void VulkanExampleBase::mouseDragged(float x, float y)
{
	handleMouseMove(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
}

void VulkanExampleBase::windowWillResize(float x, float y)
{
	resizing = true;
	if (prepared)
	{
		destWidth = x;
		destHeight = y;
		windowResize();
	}
}

void VulkanExampleBase::windowDidResize()
{
	resizing = false;
}
#elif defined(_DIRECT2DISPLAY)
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
IDirectFBSurface *VulkanExampleBase::setupWindow()
{
	DFBResult ret;
	int posx = 0, posy = 0;

	ret = DirectFBInit(NULL, NULL);
	if (ret)
	{
		std::cout << "Could not initialize DirectFB!\n";
		fflush(stdout);
		exit(1);
	}

	ret = DirectFBCreate(&dfb);
	if (ret)
	{
		std::cout << "Could not create main interface of DirectFB!\n";
		fflush(stdout);
		exit(1);
	}

	ret = dfb->GetDisplayLayer(dfb, DLID_PRIMARY, &layer);
	if (ret)
	{
		std::cout << "Could not get DirectFB display layer interface!\n";
		fflush(stdout);
		exit(1);
	}

	DFBDisplayLayerConfig layer_config;
	ret = layer->GetConfiguration(layer, &layer_config);
	if (ret)
	{
		std::cout << "Could not get DirectFB display layer configuration!\n";
		fflush(stdout);
		exit(1);
	}

	if (settings.fullscreen)
	{
		width = layer_config.width;
		height = layer_config.height;
	}
	else
	{
		if (layer_config.width > width)
			posx = (layer_config.width - width) / 2;
		if (layer_config.height > height)
			posy = (layer_config.height - height) / 2;
	}

	DFBWindowDescription desc;
	desc.flags = (DFBWindowDescriptionFlags)(DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_POSX | DWDESC_POSY);
	desc.width = width;
	desc.height = height;
	desc.posx = posx;
	desc.posy = posy;
	ret = layer->CreateWindow(layer, &desc, &window);
	if (ret)
	{
		std::cout << "Could not create DirectFB window interface!\n";
		fflush(stdout);
		exit(1);
	}

	ret = window->GetSurface(window, &surface);
	if (ret)
	{
		std::cout << "Could not get DirectFB surface interface!\n";
		fflush(stdout);
		exit(1);
	}

	ret = window->CreateEventBuffer(window, &event_buffer);
	if (ret)
	{
		std::cout << "Could not create DirectFB event buffer interface!\n";
		fflush(stdout);
		exit(1);
	}

	ret = window->SetOpacity(window, 0xFF);
	if (ret)
	{
		std::cout << "Could not set DirectFB window opacity!\n";
		fflush(stdout);
		exit(1);
	}

	return surface;
}

void VulkanExampleBase::handleEvent(const DFBWindowEvent *event)
{
	switch (event->type)
	{
	case DWET_CLOSE:
		quit = true;
		break;
	case DWET_MOTION:
		handleMouseMove(event->x, event->y);
		break;
	case DWET_BUTTONDOWN:
		switch (event->button)
		{
		case DIBI_LEFT:
			mouseButtons.left = true;
			break;
		case DIBI_MIDDLE:
			mouseButtons.middle = true;
			break;
		case DIBI_RIGHT:
			mouseButtons.right = true;
			break;
		default:
			break;
		}
		break;
	case DWET_BUTTONUP:
		switch (event->button)
		{
		case DIBI_LEFT:
			mouseButtons.left = false;
			break;
		case DIBI_MIDDLE:
			mouseButtons.middle = false;
			break;
		case DIBI_RIGHT:
			mouseButtons.right = false;
			break;
		default:
			break;
		}
		break;
	case DWET_KEYDOWN:
		switch (event->key_symbol)
		{
			case KEY_W:
				camera.keys.up = true;
				break;
			case KEY_S:
				camera.keys.down = true;
				break;
			case KEY_A:
				camera.keys.left = true;
				break;
			case KEY_D:
				camera.keys.right = true;
				break;
			case KEY_P:
				paused = !paused;
				break;
			case KEY_F1:
				if (settings.overlay) {
					settings.overlay = !settings.overlay;
				}
				break;
			default:
				break;
		}
		break;
	case DWET_KEYUP:
		switch (event->key_symbol)
		{
			case KEY_W:
				camera.keys.up = false;
				break;
			case KEY_S:
				camera.keys.down = false;
				break;
			case KEY_A:
				camera.keys.left = false;
				break;
			case KEY_D:
				camera.keys.right = false;
				break;
			case KEY_ESCAPE:
				quit = true;
				break;
			default:
				break;
		}
		keyPressed(event->key_symbol);
		break;
	case DWET_SIZE:
		destWidth = event->w;
		destHeight = event->h;
		windowResize();
		break;
	default:
		break;
	}
}
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
/*static*/void VulkanExampleBase::registryGlobalCb(void *data,
		wl_registry *registry, uint32_t name, const char *interface,
		uint32_t version)
{
	VulkanExampleBase *self = reinterpret_cast<VulkanExampleBase *>(data);
	self->registryGlobal(registry, name, interface, version);
}

/*static*/void VulkanExampleBase::seatCapabilitiesCb(void *data, wl_seat *seat,
		uint32_t caps)
{
	VulkanExampleBase *self = reinterpret_cast<VulkanExampleBase *>(data);
	self->seatCapabilities(seat, caps);
}

/*static*/void VulkanExampleBase::pointerEnterCb(void *data,
		wl_pointer *pointer, uint32_t serial, wl_surface *surface,
		wl_fixed_t sx, wl_fixed_t sy)
{
}

/*static*/void VulkanExampleBase::pointerLeaveCb(void *data,
		wl_pointer *pointer, uint32_t serial, wl_surface *surface)
{
}

/*static*/void VulkanExampleBase::pointerMotionCb(void *data,
		wl_pointer *pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
	VulkanExampleBase *self = reinterpret_cast<VulkanExampleBase *>(data);
	self->pointerMotion(pointer, time, sx, sy);
}
void VulkanExampleBase::pointerMotion(wl_pointer *pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
	handleMouseMove(wl_fixed_to_int(sx), wl_fixed_to_int(sy));
}

/*static*/void VulkanExampleBase::pointerButtonCb(void *data,
		wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button,
		uint32_t state)
{
	VulkanExampleBase *self = reinterpret_cast<VulkanExampleBase *>(data);
	self->pointerButton(pointer, serial, time, button, state);
}

void VulkanExampleBase::pointerButton(struct wl_pointer *pointer,
		uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
	switch (button)
	{
	case BTN_LEFT:
		mouseButtons.left = !!state;
		break;
	case BTN_MIDDLE:
		mouseButtons.middle = !!state;
		break;
	case BTN_RIGHT:
		mouseButtons.right = !!state;
		break;
	default:
		break;
	}
}

/*static*/void VulkanExampleBase::pointerAxisCb(void *data,
		wl_pointer *pointer, uint32_t time, uint32_t axis,
		wl_fixed_t value)
{
	VulkanExampleBase *self = reinterpret_cast<VulkanExampleBase *>(data);
	self->pointerAxis(pointer, time, axis, value);
}

void VulkanExampleBase::pointerAxis(wl_pointer *pointer, uint32_t time,
		uint32_t axis, wl_fixed_t value)
{
	double d = wl_fixed_to_double(value);
	switch (axis)
	{
	case REL_X:
		camera.translate(glm::vec3(0.0f, 0.0f, d * 0.005f));
		viewUpdated = true;
		break;
	default:
		break;
	}
}

/*static*/void VulkanExampleBase::keyboardKeymapCb(void *data,
		struct wl_keyboard *keyboard, uint32_t format, int fd, uint32_t size)
{
}

/*static*/void VulkanExampleBase::keyboardEnterCb(void *data,
		struct wl_keyboard *keyboard, uint32_t serial,
		struct wl_surface *surface, struct wl_array *keys)
{
}

/*static*/void VulkanExampleBase::keyboardLeaveCb(void *data,
		struct wl_keyboard *keyboard, uint32_t serial,
		struct wl_surface *surface)
{
}

/*static*/void VulkanExampleBase::keyboardKeyCb(void *data,
		struct wl_keyboard *keyboard, uint32_t serial, uint32_t time,
		uint32_t key, uint32_t state)
{
	VulkanExampleBase *self = reinterpret_cast<VulkanExampleBase *>(data);
	self->keyboardKey(keyboard, serial, time, key, state);
}

void VulkanExampleBase::keyboardKey(struct wl_keyboard *keyboard,
		uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
	switch (key)
	{
	case KEY_W:
		camera.keys.up = !!state;
		break;
	case KEY_S:
		camera.keys.down = !!state;
		break;
	case KEY_A:
		camera.keys.left = !!state;
		break;
	case KEY_D:
		camera.keys.right = !!state;
		break;
	case KEY_P:
		if (state)
			paused = !paused;
		break;
	case KEY_F1:
		if (state && settings.overlay)
			settings.overlay = !settings.overlay;
		break;
	case KEY_ESC:
		quit = true;
		break;
	}

	if (state)
		keyPressed(key);
}

/*static*/void VulkanExampleBase::keyboardModifiersCb(void *data,
		struct wl_keyboard *keyboard, uint32_t serial, uint32_t mods_depressed,
		uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
}

void VulkanExampleBase::seatCapabilities(wl_seat *seat, uint32_t caps)
{
	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !pointer)
	{
		pointer = wl_seat_get_pointer(seat);
		static const struct wl_pointer_listener pointer_listener =
		{ pointerEnterCb, pointerLeaveCb, pointerMotionCb, pointerButtonCb,
				pointerAxisCb, };
		wl_pointer_add_listener(pointer, &pointer_listener, this);
	}
	else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && pointer)
	{
		wl_pointer_destroy(pointer);
		pointer = nullptr;
	}

	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !keyboard)
	{
		keyboard = wl_seat_get_keyboard(seat);
		static const struct wl_keyboard_listener keyboard_listener =
		{ keyboardKeymapCb, keyboardEnterCb, keyboardLeaveCb, keyboardKeyCb,
				keyboardModifiersCb, };
		wl_keyboard_add_listener(keyboard, &keyboard_listener, this);
	}
	else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && keyboard)
	{
		wl_keyboard_destroy(keyboard);
		keyboard = nullptr;
	}
}

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *shell, uint32_t serial)
{
	xdg_wm_base_pong(shell, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
	xdg_wm_base_ping,
};

void VulkanExampleBase::registryGlobal(wl_registry *registry, uint32_t name,
		const char *interface, uint32_t version)
{
	if (strcmp(interface, "wl_compositor") == 0)
	{
		compositor = (wl_compositor *) wl_registry_bind(registry, name,
				&wl_compositor_interface, 3);
	}
	else if (strcmp(interface, "xdg_wm_base") == 0)
	{
		shell = (xdg_wm_base *) wl_registry_bind(registry, name,
				&xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(shell, &xdg_wm_base_listener, nullptr);
	}
	else if (strcmp(interface, "wl_seat") == 0)
	{
		seat = (wl_seat *) wl_registry_bind(registry, name, &wl_seat_interface,
				1);

		static const struct wl_seat_listener seat_listener =
		{ seatCapabilitiesCb, };
		wl_seat_add_listener(seat, &seat_listener, this);
	}
}

/*static*/void VulkanExampleBase::registryGlobalRemoveCb(void *data,
		struct wl_registry *registry, uint32_t name)
{
}

void VulkanExampleBase::initWaylandConnection()
{
	display = wl_display_connect(NULL);
	if (!display)
	{
		std::cout << "Could not connect to Wayland display!\n";
		fflush(stdout);
		exit(1);
	}

	registry = wl_display_get_registry(display);
	if (!registry)
	{
		std::cout << "Could not get Wayland registry!\n";
		fflush(stdout);
		exit(1);
	}

	static const struct wl_registry_listener registry_listener =
	{ registryGlobalCb, registryGlobalRemoveCb };
	wl_registry_add_listener(registry, &registry_listener, this);
	wl_display_dispatch(display);
	wl_display_roundtrip(display);
	if (!compositor || !shell || !seat)
	{
		std::cout << "Could not bind Wayland protocols!\n";
		fflush(stdout);
		exit(1);
	}
}

void VulkanExampleBase::setSize(int width, int height)
{
	if (width <= 0 || height <= 0)
		return;

	destWidth = width;
	destHeight = height;

	windowResize();
}

static void
xdg_surface_handle_configure(void *data, struct xdg_surface *surface,
			     uint32_t serial)
{
	VulkanExampleBase *base = (VulkanExampleBase *) data;

	xdg_surface_ack_configure(surface, serial);
	base->configured = true;
}

static const struct xdg_surface_listener xdg_surface_listener = {
	xdg_surface_handle_configure,
};


static void
xdg_toplevel_handle_configure(void *data, struct xdg_toplevel *toplevel,
			      int32_t width, int32_t height,
			      struct wl_array *states)
{
	VulkanExampleBase *base = (VulkanExampleBase *) data;

	base->setSize(width, height);
}

static void
xdg_toplevel_handle_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
	VulkanExampleBase *base = (VulkanExampleBase *) data;

	base->quit = true;
}


static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	xdg_toplevel_handle_configure,
	xdg_toplevel_handle_close,
};


struct xdg_surface *VulkanExampleBase::setupWindow()
{
	surface = wl_compositor_create_surface(compositor);
	xdg_surface = xdg_wm_base_get_xdg_surface(shell, surface);

	xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, this);
	xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);
	xdg_toplevel_add_listener(xdg_toplevel, &xdg_toplevel_listener, this);

	std::string windowTitle = getWindowTitle();
	xdg_toplevel_set_title(xdg_toplevel, windowTitle.c_str());
	wl_surface_commit(surface);
	return xdg_surface;
}

#elif defined(VK_USE_PLATFORM_XCB_KHR)

static inline xcb_intern_atom_reply_t* intern_atom_helper(xcb_connection_t *conn, bool only_if_exists, const char *str)
{
	xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, only_if_exists, strlen(str), str);
	return xcb_intern_atom_reply(conn, cookie, NULL);
}

// Set up a window using XCB and request event types
xcb_window_t VulkanExampleBase::setupWindow()
{
	uint32_t value_mask, value_list[32];

	window = xcb_generate_id(connection);

	value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	value_list[0] = screen->black_pixel;
	value_list[1] =
		XCB_EVENT_MASK_KEY_RELEASE |
		XCB_EVENT_MASK_KEY_PRESS |
		XCB_EVENT_MASK_EXPOSURE |
		XCB_EVENT_MASK_STRUCTURE_NOTIFY |
		XCB_EVENT_MASK_POINTER_MOTION |
		XCB_EVENT_MASK_BUTTON_PRESS |
		XCB_EVENT_MASK_BUTTON_RELEASE;

	if (settings.fullscreen)
	{
		width = destWidth = screen->width_in_pixels;
		height = destHeight = screen->height_in_pixels;
	}

	xcb_create_window(connection,
		XCB_COPY_FROM_PARENT,
		window, screen->root,
		0, 0, width, height, 0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT,
		screen->root_visual,
		value_mask, value_list);

	/* Magic code that will send notification when window is destroyed */
	xcb_intern_atom_reply_t* reply = intern_atom_helper(connection, true, "WM_PROTOCOLS");
	atom_wm_delete_window = intern_atom_helper(connection, false, "WM_DELETE_WINDOW");

	xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
		window, (*reply).atom, 4, 32, 1,
		&(*atom_wm_delete_window).atom);

	std::string windowTitle = getWindowTitle();
	xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
		window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
		title.size(), windowTitle.c_str());

	free(reply);

	/**
	 * Set the WM_CLASS property to display
	 * title in dash tooltip and application menu
	 * on GNOME and other desktop environments
	 */
	std::string wm_class;
	wm_class = wm_class.insert(0, name);
	wm_class = wm_class.insert(name.size(), 1, '\0');
	wm_class = wm_class.insert(name.size() + 1, title);
	wm_class = wm_class.insert(wm_class.size(), 1, '\0');
	xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, wm_class.size() + 2, wm_class.c_str());

	if (settings.fullscreen)
	{
		xcb_intern_atom_reply_t *atom_wm_state = intern_atom_helper(connection, false, "_NET_WM_STATE");
		xcb_intern_atom_reply_t *atom_wm_fullscreen = intern_atom_helper(connection, false, "_NET_WM_STATE_FULLSCREEN");
		xcb_change_property(connection,
				XCB_PROP_MODE_REPLACE,
				window, atom_wm_state->atom,
				XCB_ATOM_ATOM, 32, 1,
				&(atom_wm_fullscreen->atom));
		free(atom_wm_fullscreen);
		free(atom_wm_state);
	}

	xcb_map_window(connection, window);

	return(window);
}

// Initialize XCB connection
void VulkanExampleBase::initxcbConnection()
{
	const xcb_setup_t *setup;
	xcb_screen_iterator_t iter;
	int scr;

	// xcb_connect always returns a non-NULL pointer to a xcb_connection_t,
	// even on failure. Callers need to use xcb_connection_has_error() to
	// check for failure. When finished, use xcb_disconnect() to close the
	// connection and free the structure.
	connection = xcb_connect(NULL, &scr);
	assert( connection );
	if( xcb_connection_has_error(connection) ) {
		printf("Could not find a compatible Vulkan ICD!\n");
		fflush(stdout);
		exit(1);
	}

	setup = xcb_get_setup(connection);
	iter = xcb_setup_roots_iterator(setup);
	while (scr-- > 0)
		xcb_screen_next(&iter);
	screen = iter.data;
}

void VulkanExampleBase::handleEvent(const xcb_generic_event_t *event)
{
	switch (event->response_type & 0x7f)
	{
	case XCB_CLIENT_MESSAGE:
		if ((*(xcb_client_message_event_t*)event).data.data32[0] ==
			(*atom_wm_delete_window).atom) {
			quit = true;
		}
		break;
	case XCB_MOTION_NOTIFY:
	{
		xcb_motion_notify_event_t *motion = (xcb_motion_notify_event_t *)event;
		handleMouseMove((int32_t)motion->event_x, (int32_t)motion->event_y);
		break;
	}
	break;
	case XCB_BUTTON_PRESS:
	{
		xcb_button_press_event_t *press = (xcb_button_press_event_t *)event;
		if (press->detail == XCB_BUTTON_INDEX_1)
			mouseButtons.left = true;
		if (press->detail == XCB_BUTTON_INDEX_2)
			mouseButtons.middle = true;
		if (press->detail == XCB_BUTTON_INDEX_3)
			mouseButtons.right = true;
	}
	break;
	case XCB_BUTTON_RELEASE:
	{
		xcb_button_press_event_t *press = (xcb_button_press_event_t *)event;
		if (press->detail == XCB_BUTTON_INDEX_1)
			mouseButtons.left = false;
		if (press->detail == XCB_BUTTON_INDEX_2)
			mouseButtons.middle = false;
		if (press->detail == XCB_BUTTON_INDEX_3)
			mouseButtons.right = false;
	}
	break;
	case XCB_KEY_PRESS:
	{
		const xcb_key_release_event_t *keyEvent = (const xcb_key_release_event_t *)event;
		switch (keyEvent->detail)
		{
			case KEY_W:
				camera.keys.up = true;
				break;
			case KEY_S:
				camera.keys.down = true;
				break;
			case KEY_A:
				camera.keys.left = true;
				break;
			case KEY_D:
				camera.keys.right = true;
				break;
			case KEY_P:
				paused = !paused;
				break;
			case KEY_F1:
				if (settings.overlay) {
					settings.overlay = !settings.overlay;
				}
				break;
		}
	}
	break;
	case XCB_KEY_RELEASE:
	{
		const xcb_key_release_event_t *keyEvent = (const xcb_key_release_event_t *)event;
		switch (keyEvent->detail)
		{
			case KEY_W:
				camera.keys.up = false;
				break;
			case KEY_S:
				camera.keys.down = false;
				break;
			case KEY_A:
				camera.keys.left = false;
				break;
			case KEY_D:
				camera.keys.right = false;
				break;
			case KEY_ESCAPE:
				quit = true;
				break;
		}
		keyPressed(keyEvent->detail);
	}
	break;
	case XCB_DESTROY_NOTIFY:
		quit = true;
		break;
	case XCB_CONFIGURE_NOTIFY:
	{
		const xcb_configure_notify_event_t *cfgEvent = (const xcb_configure_notify_event_t *)event;
		if ((prepared) && ((cfgEvent->width != width) || (cfgEvent->height != height)))
		{
				destWidth = cfgEvent->width;
				destHeight = cfgEvent->height;
				if ((destWidth > 0) && (destHeight > 0))
				{
					windowResize();
				}
		}
	}
	break;
	default:
		break;
	}
}
#else
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
#endif

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
#if defined(_WIN32)
	swapChain.initSurface(windowInstance, window);
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
	swapChain.initSurface(view);
#elif defined(_DIRECT2DISPLAY)
	swapChain.initSurface(width, height);
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
	swapChain.initSurface(dfb, surface);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
	swapChain.initSurface(display, surface);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	swapChain.initSurface(connection, window);
#else
	swapChain.initSurface(window);
#endif
}

void VulkanExampleBase::setupSwapChain()
{
	swapChain.create(&width, &height, settings.vsync);
}

void VulkanExampleBase::OnUpdateUIOverlay(vks::UIOverlay *overlay) {}
