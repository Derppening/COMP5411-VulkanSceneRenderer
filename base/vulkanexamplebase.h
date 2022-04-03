/*
* Vulkan Example base class
*
* Copyright (C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <array>
#include <unordered_map>
#include <numeric>
#include <ctime>
#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>
#include <sys/stat.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <numeric>
#include <array>
#include <memory>

#include <vulkan/vulkan.hpp>

#include "VulkanTools.h"
#include "VulkanDebug.h"
#include "VulkanUIOverlay.h"
#include "VulkanSwapChain.h"
#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanTexture.h"

#include "VulkanInitializers.hpp"
#include "camera.hpp"

class CommandLineParser
{
public:
	struct CommandLineOption {
		std::vector<std::string> commands;
		std::string value;
		bool hasValue = false;
		std::string help;
		bool set = false;
	};
	std::unordered_map<std::string, CommandLineOption> options;
	CommandLineParser();
	void add(const std::string& name, const std::vector<std::string>& commands, bool hasValue, const std::string& help);
	void printHelp();
	void parse(const std::vector<const char*>& arguments);
	bool isSet(const std::string& name);
	std::string getValueAsString(const std::string& name, const std::string& defaultValue);
	std::int32_t getValueAsInt(const std::string& name, std::int32_t defaultValue);
};

class VulkanExampleBase
{
private:
	std::string getWindowTitle();
	bool viewUpdated = false;
	uint32_t destWidth;
	uint32_t destHeight;
	bool resizing = false;
	void windowResize();
	void handleMouseMove(int32_t x, int32_t y);
	void nextFrame();
	void updateOverlay();
	void createPipelineCache();
	void createCommandPool();
	void createSynchronizationPrimitives();
	void initSwapchain();
	void setupSwapChain();
	void createCommandBuffers();
	void destroyCommandBuffers();
public:
	// Returns the path to the root of the glsl or hlsl shader directory.
	std::string getShadersPath() const;

	// Frame counter to display fps
	uint32_t frameCounter = 0;
	uint32_t lastFPS = 0;
	std::chrono::time_point<std::chrono::high_resolution_clock> lastTimestamp;
	// Vulkan instance, stores all per-application states
	vk::UniqueInstance instance;
	std::vector<std::string> supportedInstanceExtensions;
	// Physical device (GPU) that Vulkan will use
	vk::PhysicalDevice physicalDevice;
	// Stores physical device properties (for e.g. checking device limits)
	vk::PhysicalDeviceProperties2 deviceProperties;
	// Stores the features available on the selected physical device (for e.g. checking if a feature is available)
	vk::PhysicalDeviceFeatures2 deviceFeatures;
	// Stores all available memory (type) properties for the physical device
	vk::PhysicalDeviceMemoryProperties2 deviceMemoryProperties;
	/** @brief Set of physical device features to be enabled for this example (must be set in the derived constructor) */
	vk::PhysicalDeviceFeatures2 enabledFeatures{};
	/** @brief Set of device extensions to be enabled for this example (must be set in the derived constructor) */
	std::vector<const char*> enabledDeviceExtensions;
	std::vector<const char*> enabledInstanceExtensions;
	/** @brief Optional pNext structure for passing extension structures to device creation */
	void* deviceCreatepNextChain = nullptr;
	/** @brief Logical device, application's view of the physical device (GPU) */
	vk::Device device;
	// Handle to the device graphics queue that command buffers are submitted to
	vk::Queue queue;
	// Depth buffer format (selected during Vulkan initialization)
	vk::Format depthFormat;
	// Command buffer pool
	vk::UniqueCommandPool cmdPool;
	/** @brief Pipeline stages used to wait at for graphics queue submissions */
	vk::PipelineStageFlags submitPipelineStages = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	// Contains command buffers and semaphores to be presented to the queue
	vk::SubmitInfo submitInfo;
	// Command buffers used for rendering
	std::vector<vk::UniqueCommandBuffer> drawCmdBuffers;
	// Global render pass for frame buffer writes
	vk::UniqueRenderPass renderPass;
	// List of available frame buffers (same as number of swap chain images)
	std::vector<vk::UniqueFramebuffer>frameBuffers;
	// Active frame buffer index
	uint32_t currentBuffer = 0;
	// Descriptor set pool
	vk::UniqueDescriptorPool descriptorPool;
	// List of shader modules created (stored for cleanup)
	std::vector<vk::UniqueShaderModule> shaderModules;
	// Pipeline cache object
	vk::UniquePipelineCache pipelineCache;
	// Wraps the swap chain to present images (framebuffers) to the windowing system
	VulkanSwapChain swapChain;
	// Synchronization semaphores
	struct {
		// Swap chain image presentation
		vk::UniqueSemaphore presentComplete;
		// Command buffer submission and execution
		vk::UniqueSemaphore renderComplete;
	} semaphores;
	std::vector<vk::UniqueFence> waitFences;

	bool prepared = false;
	bool resized = false;
	uint32_t width = 1280;
	uint32_t height = 720;

	vks::UIOverlay UIOverlay;
    CommandLineParser commandLineParser;

	/** @brief Last frame time measured using a high performance timer (if available) */
	float frameTimer = 1.0f;

	/** @brief Encapsulated physical and logical vulkan device */
	std::unique_ptr<vks::VulkanDevice> vulkanDevice;

	/** @brief Example settings that can be changed e.g. by command line arguments */
	struct Settings {
		/** @brief Activates validation layers (and message output) when set to true */
		bool validation = false;
		/** @brief Set to true if fullscreen mode has been requested via command line */
		bool fullscreen = false;
		/** @brief Set to true if v-sync will be forced for the swapchain */
		bool vsync = false;
		/** @brief Enable UI overlay */
		bool overlay = true;
	} settings;

	vk::ClearColorValue defaultClearColor = { std::array{ 0.025f, 0.025f, 0.025f, 1.0f } };

	static std::vector<const char*> args;

	// Defines a frame rate independent timer value clamped from -1.0...1.0
	// For use in animations, rotations, etc.
	float timer = 0.0f;
	// Multiplier for speeding up (or slowing down) the global timer
	float timerSpeed = 0.25f;
	bool paused = false;

	Camera camera;
	glm::vec2 mousePos;

	std::string title = "Vulkan Example";
	std::string name = "vulkanExample";
	uint32_t apiVersion = VK_API_VERSION_1_2;

	struct {
		vk::UniqueImage image;
		vk::UniqueDeviceMemory mem;
		vk::UniqueImageView view;
	} depthStencil;

	struct {
		glm::vec2 axisLeft = glm::vec2(0.0f);
		glm::vec2 axisRight = glm::vec2(0.0f);
	} gamePadState;

	struct {
		bool left = false;
		bool right = false;
		bool middle = false;
	} mouseButtons;

	// OS specific
	GLFWwindow* window;

	VulkanExampleBase(bool enableValidation = false);
	virtual ~VulkanExampleBase();
	/** @brief Setup the vulkan instance, enable required extensions and connect to the physical device (GPU) */
	bool initVulkan();

	GLFWwindow* setupWindow();
	void processInput();
    static void cursorCallback(GLFWwindow* window, double xpos, double ypos);
    static void mouseBtnCallback(GLFWwindow* window, int button, int action, int mods);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void windowResizeCallback(GLFWwindow* window, int width, int height);

	/** @brief (Virtual) Creates the application wide Vulkan instance */
	virtual void createInstance(bool enableValidation);
	/** @brief (Pure virtual) Render function to be implemented by the sample application */
	virtual void render() = 0;
	/** @brief (Virtual) Called when the camera view has changed */
	virtual void viewChanged();
	/** @brief (Virtual) Called after a key was pressed, can be used to do custom key handling */
	virtual void keyPressed(uint32_t);
	/** @brief (Virtual) Called after the mouse cursor moved and before internal events (like camera rotation) is handled */
	virtual void mouseMoved(double x, double y, bool &handled);
	/** @brief (Virtual) Called when the window has been resized, can be used by the sample application to recreate resources */
	virtual void windowResized();
	/** @brief (Virtual) Called when resources have been recreated that require a rebuild of the command buffers (e.g. frame buffer), to be implemented by the sample application */
	virtual void buildCommandBuffers();
	/** @brief (Virtual) Setup default depth and stencil views */
	virtual void setupDepthStencil();
	/** @brief (Virtual) Setup default framebuffers for all requested swapchain images */
	virtual void setupFrameBuffer();
	/** @brief (Virtual) Setup a default renderpass */
	virtual void setupRenderPass();
	/** @brief (Virtual) Called after the physical device features have been read, can be used to set features to enable on the device */
	virtual void getEnabledFeatures();

	/** @brief Prepares all Vulkan resources and functions required to run the sample */
	virtual void prepare();

	/** @brief Loads a SPIR-V shader file for the given shader stage */
	vk::PipelineShaderStageCreateInfo loadShader(std::string fileName, vk::ShaderStageFlagBits stage);

	/** @brief Entry point for the main render loop */
	void renderLoop();

	/** @brief Adds the drawing commands for the ImGui overlay to the given command buffer */
	void drawUI(const vk::CommandBuffer commandBuffer);

	/** Prepare the next frame for workload submission by acquiring the next swap chain image */
	void prepareFrame();
	/** @brief Presents the current image to the swap chain */
	void submitFrame();
	/** @brief (Virtual) Default image acquire + submission and command buffer submission function */
	virtual void renderFrame();

	/** @brief (Virtual) Called when the UI overlay is updating, can be used to add custom elements to the overlay */
	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay);
};

// OS specific macros for the example main entry points
#define VULKAN_EXAMPLE_MAIN()																		\
VulkanExample *vulkanExample;																		\
int main(const int argc, const char *argv[])													    \
{																									\
	for (size_t i = 0; i < argc; i++) { VulkanExample::args.push_back(argv[i]); };  				\
	vulkanExample = new VulkanExample();															\
	vulkanExample->setupWindow();					 												\
	vulkanExample->initVulkan();																	\
	vulkanExample->prepare();																		\
	vulkanExample->renderLoop();																	\
	delete(vulkanExample);																			\
	return 0;																						\
}
