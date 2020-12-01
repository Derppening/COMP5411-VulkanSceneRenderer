#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_EXTERNAL_IMAGE

#include "main.h"

#include <fmt/format.h>

constexpr bool ENABLE_VALIDATION = false;

vulkan_scene_renderer::vulkan_scene_renderer() : VulkanExampleBase(ENABLE_VALIDATION) {
  title = "Vulkan Scene Renderer";
  camera.type = Camera::CameraType::firstperson;
  camera.flipY = true;
  camera.setPosition(glm::vec3{0.0f, 1.0f, 0.0f});
  camera.setRotation(glm::vec3{0.0f, -90.0f, 0.0f});
  camera.setPerspective(60.0f, static_cast<float>(width) / static_cast<float>(height), 0.1f, 256.0f);
  settings.overlay = true;
}

vulkan_scene_renderer::~vulkan_scene_renderer() {
  _screenshot_.unbind();

  _light_cube_.unbind();
  _gs_pipeline_.unbind();
  _ts_.unbind();
  _pipeline_layout_.reset();
  descriptor_set_layouts.textures.reset();

  _depth_ms_target_.unbind();
  _color_ms_target_.unbind();

  _light_ubo_.destroy();
  _settings_ubo_.destroy();
  _matrices_ubo_.destroy();
  _query_pool_.unbind();
}

void vulkan_scene_renderer::getEnabledFeatures() {
  enabledFeatures.sampleRateShading = deviceFeatures.sampleRateShading;
  enabledFeatures.samplerAnisotropy = deviceFeatures.samplerAnisotropy;
  enabledFeatures.geometryShader = deviceFeatures.geometryShader;
  enabledFeatures.tessellationShader = deviceFeatures.tessellationShader;
  enabledFeatures.pipelineStatisticsQuery = deviceFeatures.pipelineStatisticsQuery;
  enabledFeatures.fillModeNonSolid = deviceFeatures.fillModeNonSolid;
}

void vulkan_scene_renderer::buildCommandBuffers() {
  vk::CommandBufferBeginInfo cmd_buf_info = vks::initializers::commandBufferBeginInfo();

  std::vector<vk::ClearValue> clear_values;
  clear_values.emplace_back(std::array{0.25f, 0.25f, 0.25f, 1.0f });
  if (_sample_count_ != vk::SampleCountFlagBits::e1) {
    clear_values.emplace_back(std::array{0.25f, 0.25f, 0.25f, 1.0f});
  }
  clear_values.emplace_back(vk::ClearDepthStencilValue{1.0f, 0});

  vk::RenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
  renderPassBeginInfo.renderPass = *renderPass;
  renderPassBeginInfo.renderArea.offset.x = 0;
  renderPassBeginInfo.renderArea.offset.y = 0;
  renderPassBeginInfo.renderArea.extent.width = width;
  renderPassBeginInfo.renderArea.extent.height = height;
  renderPassBeginInfo.clearValueCount = clear_values.size();
  renderPassBeginInfo.pClearValues = clear_values.data();

  const vk::Viewport viewport = vks::initializers::viewport(static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
  const vk::Rect2D scissor = vks::initializers::rect2D(static_cast<std::int32_t>(width), static_cast<std::int32_t>(height), 0, 0);

  for (std::size_t i = 0; i < drawCmdBuffers.size(); ++i) {
    renderPassBeginInfo.framebuffer = *frameBuffers[i];
    drawCmdBuffers[i]->begin(cmd_buf_info);

    _query_pool_.reset(*drawCmdBuffers[i]);

    drawCmdBuffers[i]->beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
    drawCmdBuffers[i]->setViewport(0, {viewport});
    drawCmdBuffers[i]->setScissor(0, {scissor});

    drawCmdBuffers[i]->setLineWidth(1.0f);

    _query_pool_.begin(*drawCmdBuffers[i]);

    // Bind scene matrices descriptor to set 0
    drawCmdBuffers[i]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *_pipeline_layout_, 0, {_matrices_ubo_.descriptor_set()}, {});
    // Bind settings descriptor to set 2
    drawCmdBuffers[i]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *_pipeline_layout_, 2, {_settings_ubo_.descriptor_set()}, {});
    drawCmdBuffers[i]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *_pipeline_layout_, 3, {_light_ubo_.descriptor_set()}, {});

    // POI: Draw the glTF scene
    if (_draw_scene_) {
      _gltf_scene_.draw(*drawCmdBuffers[i], *_pipeline_layout_);

    }
    if (_gs_pipeline_.enabled()) {
      _gltf_scene_.draw(*drawCmdBuffers[i], *_pipeline_layout_, _gs_pipeline_.pipeline());
    }

    if (_draw_light_) {
      _light_cube_.draw(*drawCmdBuffers[i]);
    }

    _query_pool_.end(*drawCmdBuffers[i]);

    drawUI(*drawCmdBuffers[i]);
    drawCmdBuffers[i]->endRenderPass();
    drawCmdBuffers[i]->end();
  }
}

void vulkan_scene_renderer::setupRenderPass() {
  if (_sample_count_ == vk::SampleCountFlagBits::e1) {
    VulkanExampleBase::setupRenderPass();
  } else {
    std::array<vk::AttachmentDescription, 3> attachments = {};

    // Multisampled attachment that we render to
    attachments[0].format = swapChain.colorFormat;
    attachments[0].samples = _sample_count_;
    attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
    attachments[0].storeOp = vk::AttachmentStoreOp::eDontCare;
    attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachments[0].initialLayout = vk::ImageLayout::eUndefined;
    attachments[0].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

    // Framebuffer attachment where multisampled image will be resolved and presented to the swapchain
    attachments[1].format = swapChain.colorFormat;
    attachments[1].samples = vk::SampleCountFlagBits::e1;
    attachments[1].loadOp = vk::AttachmentLoadOp::eDontCare;
    attachments[1].storeOp = vk::AttachmentStoreOp::eStore;
    attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachments[1].initialLayout = vk::ImageLayout::eUndefined;
    attachments[1].finalLayout = vk::ImageLayout::ePresentSrcKHR;

    // Multisampled depth attachment
    attachments[2].format = depthFormat;
    attachments[2].samples = _sample_count_;
    attachments[2].loadOp = vk::AttachmentLoadOp::eClear;
    attachments[2].storeOp = vk::AttachmentStoreOp::eDontCare;
    attachments[2].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachments[2].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachments[2].initialLayout = vk::ImageLayout::eUndefined;
    attachments[2].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::AttachmentReference color_reference = {};
    color_reference.attachment = 0;
    color_reference.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::AttachmentReference depth_reference = {};
    depth_reference.attachment = 2;
    depth_reference.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    // Resolve attachment reference for the color attachment
    vk::AttachmentReference resolve_reference = {};
    resolve_reference.attachment = 1;
    resolve_reference.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::SubpassDescription subpass = {};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_reference;
    // Pass our resolve attachments to the subpass
    subpass.pResolveAttachments = &resolve_reference;
    subpass.pDepthStencilAttachment = &depth_reference;

    std::array<vk::SubpassDependency, 2> dependencies = {};

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
    dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
    dependencies[0].dstAccessMask =
        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
    dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
    dependencies[1].srcAccessMask =
        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
    dependencies[1].dstAccessMask = vk::AccessFlagBits::eMemoryRead;
    dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

    vk::RenderPassCreateInfo render_pass_info = vks::initializers::renderPassCreateInfo();
    render_pass_info.attachmentCount = attachments.size();
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 2;
    render_pass_info.pDependencies = dependencies.data();

    renderPass = device.createRenderPassUnique(render_pass_info);
  }
}

void vulkan_scene_renderer::setupFrameBuffer() {
  if (_sample_count_ == vk::SampleCountFlagBits::e1) {
    VulkanExampleBase::setupFrameBuffer();
  } else {
    std::array<vk::ImageView, 3> attachments = {};

    _setup_multisample_target();

    attachments[0] = const_cast<const image_multisample_target&>(_color_ms_target_).view();
    attachments[2] = const_cast<const depth_multisample_target&>(_depth_ms_target_).view();

    vk::FramebufferCreateInfo framebuffer_create_info = {};
    framebuffer_create_info.pNext = nullptr;
    framebuffer_create_info.renderPass = *renderPass;
    framebuffer_create_info.attachmentCount = attachments.size();
    framebuffer_create_info.pAttachments = attachments.data();
    framebuffer_create_info.width = width;
    framebuffer_create_info.height = height;
    framebuffer_create_info.layers = 1;

    // Create frame buffers for every swap chain image
    frameBuffers.resize(swapChain.imageCount);
    for (std::uint32_t i = 0; i < frameBuffers.size(); ++i) {
      attachments[1] = *swapChain.buffers[i].view;
      frameBuffers[i] = device.createFramebufferUnique(framebuffer_create_info);
    }
  }
}

void vulkan_scene_renderer::load_gltf_file(std::string filename) {
  tinygltf::Model gltf_input;
  tinygltf::TinyGLTF gltf_context;
  std::string error, warning;

  this->device = device;

  bool file_loaded = gltf_context.LoadASCIIFromFile(&gltf_input, &error, &warning, filename);

  // Pass some Vulkan resources required for setup and rendering to the glTF model loading class
  _gltf_scene_.vulkan_device = vulkanDevice.get();
  _gltf_scene_.copy_queue = queue;

  std::size_t pos = filename.find_last_of('/');
  _gltf_scene_.path = filename.substr(0, pos);

  std::vector<std::uint32_t> index_buffer;
  std::vector<vulkan_gltf_scene::vertex> vertex_buffer;

  if (file_loaded) {
    _gltf_scene_.load_images(gltf_input);
    _gltf_scene_.load_materials(gltf_input);
    _gltf_scene_.load_textures(gltf_input);
    const tinygltf::Scene& scene = gltf_input.scenes[0];
    for (int i : scene.nodes) {
      const tinygltf::Node node = gltf_input.nodes[static_cast<std::size_t>(i)];
      _gltf_scene_.load_node(node, gltf_input, nullptr, index_buffer, vertex_buffer);
    }
  } else {
    vks::tools::exitFatal("Could not open the glTF file.\n\nThe file is part of the additional asset pack.\n\nRun \"download_assets.py\" in the repository root to download the latest version.", -1);
    return;
  }

  // Create and upload vertex and index buffer
  // We will be using one single vertex buffer and one single index buffer for the whole glTF scene
  // Primitives (of the glTF model) will then index into these using index offsets

  std::size_t vertex_buffer_size = vertex_buffer.size() * sizeof(vulkan_gltf_scene::vertex);
  std::size_t index_buffer_size = index_buffer.size() * sizeof(std::uint32_t);
  _gltf_scene_.indices.count = static_cast<int>(index_buffer.size());

  vks::Buffer vertex_staging, index_staging;

  vulkanDevice->createBuffer(
      vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      &vertex_staging,
      vertex_buffer_size,
      vertex_buffer.data());

  // Index data
  vulkanDevice->createBuffer(
      vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      &index_staging,
      index_buffer_size,
      index_buffer.data());

  // Create device local buffers (target)
  vulkanDevice->createBuffer(
      vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eDeviceLocal,
      &_gltf_scene_.vertices,
      vertex_buffer_size);
  vulkanDevice->createBuffer(
      vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eDeviceLocal,
      &_gltf_scene_.indices.buffer,
      index_buffer_size);

  // Copy data from staging buffers (host) do device local buffer (gpu)
  vk::UniqueCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(vk::CommandBufferLevel::ePrimary, true);
  vk::BufferCopy copyRegion = {};

  copyRegion.size = vertex_buffer_size;
  copyCmd->copyBuffer(*vertex_staging.buffer, *_gltf_scene_.vertices.buffer, {copyRegion});

  copyRegion.size = index_buffer_size;
  copyCmd->copyBuffer(*index_staging.buffer, *_gltf_scene_.indices.buffer.buffer, {copyRegion});

  vulkanDevice->flushCommandBuffer(copyCmd, queue, true);

  // Free staging resources
  vertex_staging.destroy();
  index_staging.destroy();
}

void vulkan_scene_renderer::load_assets() {
  load_gltf_file(getAssetPath() + "models/sponza/sponza.gltf");
}

void vulkan_scene_renderer::setup_descriptors() {
  /*
		This sample uses separate descriptor sets (and layouts) for the matrices and materials (textures)
	*/

  // One ubo to pass dynamic data to the shader, one for settings, one of dir light and one for point light
  // Two combined image samplers per material as each material uses color and normal maps
  std::vector<vk::DescriptorPoolSize> pool_sizes = {
      vks::initializers::descriptorPoolSize(vk::DescriptorType::eUniformBuffer, 6),
      vks::initializers::descriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, static_cast<uint32_t>(_gltf_scene_.materials.size()) * 2),
  };
  // One set for matrices and one per model image/texture
  const uint32_t max_set_count = static_cast<uint32_t>(_gltf_scene_.images.size()) + 1;
  vk::DescriptorPoolCreateInfo descriptor_pool_info = vks::initializers::descriptorPoolCreateInfo(pool_sizes, max_set_count);
  descriptorPool = device.createDescriptorPoolUnique(descriptor_pool_info);

  // Descriptor set layout for passing matrices
  _matrices_ubo_.setup_descriptor_set_layout(device, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eTessellationEvaluation);

  // Descriptor set layout for passing material textures
  auto set_layout_bindings = std::vector{
      // Color map
      vks::initializers::descriptorSetLayoutBinding(vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 0),
      // Normal map
      vks::initializers::descriptorSetLayoutBinding(vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1),
  };
  auto descriptor_set_layout_ci = vks::initializers::descriptorSetLayoutCreateInfo(set_layout_bindings.data(), static_cast<uint32_t>(set_layout_bindings.size()));
  descriptor_set_layout_ci.pBindings = set_layout_bindings.data();
  descriptor_set_layout_ci.bindingCount = 2;
  descriptor_set_layouts.textures = device.createDescriptorSetLayoutUnique(descriptor_set_layout_ci);

  // Descriptor set layout for passing dynamic settings
  _settings_ubo_.setup_descriptor_set_layout(device, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

  _light_ubo_.setup_descriptor_set_layout(device, vk::ShaderStageFlagBits::eFragment);

  // Pipeline layout using both descriptor sets (set 0 = matrices, set 1 = material, set 2 = settings)
  auto setLayouts = std::array{
      _matrices_ubo_.descriptor_set_layout(),
      *descriptor_set_layouts.textures,
      _settings_ubo_.descriptor_set_layout(),
      _light_ubo_.descriptor_set_layout()
  };
  vk::PipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
  // We will use push constants to push the local matrices of a primitive to the vertex shader
  vk::PushConstantRange pushConstantRange = vks::initializers::pushConstantRange(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eTessellationEvaluation, sizeof(glm::mat4), 0);
  // Push constant ranges are part of the pipeline layout
  pipelineLayoutCI.pushConstantRangeCount = 1;
  pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
  _pipeline_layout_ = device.createPipelineLayoutUnique(pipelineLayoutCI);

  // Descriptor set for scene matrices
  _matrices_ubo_.setup_descriptor_sets(device, *descriptorPool);

  // Descriptor sets for materials
  for (auto& material : _gltf_scene_.materials) {
    const vk::DescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(*descriptorPool, &*descriptor_set_layouts.textures, 1);
    material.descriptor_set = device.allocateDescriptorSets(allocInfo)[0];
    vk::DescriptorImageInfo colorMap = _gltf_scene_.get_texture_descriptor(material.base_color_texture_index);
    vk::DescriptorImageInfo normalMap = _gltf_scene_.get_texture_descriptor(material.normal_texture_index);
    std::vector<vk::WriteDescriptorSet> writeDescriptorSets = {
        vks::initializers::writeDescriptorSet(material.descriptor_set, vk::DescriptorType::eCombinedImageSampler, 0, &colorMap),
        vks::initializers::writeDescriptorSet(material.descriptor_set, vk::DescriptorType::eCombinedImageSampler, 1, &normalMap),
    };
    device.updateDescriptorSets(writeDescriptorSets, {});
  }

  _settings_ubo_.setup_descriptor_sets(device, *descriptorPool);

  _light_ubo_.setup_descriptor_sets(device, *descriptorPool);
}

void vulkan_scene_renderer::prepare_pipelines() {
  vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(vk::PrimitiveTopology::eTriangleList, {}, VK_FALSE);

  vk::PipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise, {});
  if (_wireframe_) {
    rasterizationStateCI.polygonMode = vk::PolygonMode::eLine;
  }

  vk::PipelineColorBlendAttachmentState blendAttachmentStateCI = vks::initializers::pipelineColorBlendAttachmentState(vk::ColorComponentFlags{0xf}, VK_FALSE);
  vk::PipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentStateCI);
  vk::PipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, vk::CompareOp::eLessOrEqual);
  vk::PipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, {});

  vk::PipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(_sample_count_, {});
  if (enabledFeatures.sampleRateShading && _sample_count_ != vk::SampleCountFlagBits::e1 && _use_sample_shading_) {
    multisampleStateCI.sampleShadingEnable = true;
    multisampleStateCI.minSampleShading = 0.25f;
  }

  const std::vector<vk::DynamicState> dynamicStateEnables = {
      vk::DynamicState::eViewport,
      vk::DynamicState::eScissor,
      vk::DynamicState::eLineWidth
  };
  vk::PipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), {});
  std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;

  const std::vector<vk::VertexInputBindingDescription> vertexInputBindings = {
      vks::initializers::vertexInputBindingDescription(0, sizeof(vulkan_gltf_scene::vertex), vk::VertexInputRate::eVertex),
  };
  const std::vector<vk::VertexInputAttributeDescription> vertexInputAttributes = {
      vks::initializers::vertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(vulkan_gltf_scene::vertex, pos)),
      vks::initializers::vertexInputAttributeDescription(0, 1, vk::Format::eR32G32B32Sfloat, offsetof(vulkan_gltf_scene::vertex, normal)),
      vks::initializers::vertexInputAttributeDescription(0, 2, vk::Format::eR32G32B32Sfloat, offsetof(vulkan_gltf_scene::vertex, uv)),
      vks::initializers::vertexInputAttributeDescription(0, 3, vk::Format::eR32G32B32Sfloat, offsetof(vulkan_gltf_scene::vertex, color)),
      vks::initializers::vertexInputAttributeDescription(0, 4, vk::Format::eR32G32B32Sfloat, offsetof(vulkan_gltf_scene::vertex, tangent)),
  };
  vk::PipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::pipelineVertexInputStateCreateInfo(vertexInputBindings, vertexInputAttributes);
  auto tessellation_state = vks::initializers::pipelineTessellationStateCreateInfo(3);

  vk::GraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(*_pipeline_layout_, *renderPass, {});
  pipelineCI.pVertexInputState = &vertexInputStateCI;
  pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
  pipelineCI.pRasterizationState = &rasterizationStateCI;
  pipelineCI.pColorBlendState = &colorBlendStateCI;
  pipelineCI.pMultisampleState = &multisampleStateCI;
  pipelineCI.pViewportState = &viewportStateCI;
  pipelineCI.pDepthStencilState = &depthStencilStateCI;
  pipelineCI.pDynamicState = &dynamicStateCI;
  if (enabledFeatures.tessellationShader) {
    pipelineCI.pTessellationState = &tessellation_state;
  }

  _gs_pipeline_.unbind();
  _gs_pipeline_.set_pipeline_layout(*_pipeline_layout_);
  _gs_pipeline_.bind(*this);

  shaderStages.resize(2);
  pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
  pipelineCI.pStages = shaderStages.data();
  if (_shader_modules_._vert && _shader_modules_._frag) {
    shaderStages[0].stage = vk::ShaderStageFlagBits::eVertex;
    shaderStages[0].module = _shader_modules_._vert;
    shaderStages[0].pName = "main";
    shaderStages[1].stage = vk::ShaderStageFlagBits::eFragment;
    shaderStages[1].module = _shader_modules_._frag;
    shaderStages[1].pName = "main";
  } else {
    shaderStages[0] = loadShader(getShadersPath() + "gltfscenerendering/scene.vert.spv",
                                 vk::ShaderStageFlagBits::eVertex);
    shaderStages[1] = loadShader(getShadersPath() + "gltfscenerendering/scene.frag.spv",
                                 vk::ShaderStageFlagBits::eFragment);

    _shader_modules_._vert = shaderStages[0].module;
    _shader_modules_._frag = shaderStages[1].module;
  }

  if (_ts_.enabled()) {
    _ts_.populate_ci(pipelineCI, shaderStages);
  }

  // POI: Instead if using a few fixed pipelines, we create one pipeline for each material using the properties of that material
  for (auto &material : _gltf_scene_.materials) {

    struct MaterialSpecializationData {
      bool alphaMask;
      float alphaMaskCutoff;
      bool preTransformPos;
      float tessLevel;
      float tessAlpha;
    } materialSpecializationData;

    materialSpecializationData.alphaMask = material.alpha_mode == "MASK";
    materialSpecializationData.alphaMaskCutoff = material.alpha_cutoff;
    materialSpecializationData.preTransformPos = !_ts_.enabled();
    materialSpecializationData.tessLevel = _ts_.level();
    materialSpecializationData.tessAlpha = _ts_.alpha();

    // POI: Constant fragment shader material parameters will be set using specialization constants
    std::vector<vk::SpecializationMapEntry> specializationMapEntries = {
        vks::initializers::specializationMapEntry(0, offsetof(MaterialSpecializationData, alphaMask), sizeof(MaterialSpecializationData::alphaMask)),
        vks::initializers::specializationMapEntry(1, offsetof(MaterialSpecializationData, alphaMaskCutoff), sizeof(MaterialSpecializationData::alphaMaskCutoff)),
        vks::initializers::specializationMapEntry(2, offsetof(MaterialSpecializationData, preTransformPos), sizeof(MaterialSpecializationData::preTransformPos)),
        vks::initializers::specializationMapEntry(3, offsetof(MaterialSpecializationData, tessLevel), sizeof(MaterialSpecializationData::tessLevel)),
        vks::initializers::specializationMapEntry(4, offsetof(MaterialSpecializationData, tessAlpha), sizeof(MaterialSpecializationData::tessAlpha)),
    };
    vk::SpecializationInfo specializationInfo = vks::initializers::specializationInfo(specializationMapEntries, sizeof(materialSpecializationData), &materialSpecializationData);
    for (auto& ss : shaderStages) {
      ss.pSpecializationInfo = &specializationInfo;
    }

    // For double sided materials, culling will be disabled
    rasterizationStateCI.cullMode = material.double_sided ? vk::CullModeFlagBits::eNone : vk::CullModeFlagBits::eBack;

    material.pipeline = device.createGraphicsPipelineUnique(*pipelineCache, {pipelineCI}).value;
  }
}

void vulkan_scene_renderer::prepare_uniform_buffers() {
  _matrices_ubo_.prepare(*vulkanDevice, false);

  _settings_ubo_.prepare(*vulkanDevice, false);

  _light_ubo_.prepare(*vulkanDevice, false);
  _light_ubo_.update_distance(false);
  _light_ubo_.update_spot_light_radius(false);

  update_uniform_buffers();
}

void vulkan_scene_renderer::update_uniform_buffers() {
  _matrices_ubo_.values().projection = camera.matrices.perspective;
  _matrices_ubo_.values().view = camera.matrices.view;
  _matrices_ubo_.values().viewPos = camera.viewPos;
  _matrices_ubo_.update();

  _settings_ubo_.update();

  auto& spot_light = _light_ubo_.values().spot_light;
  spot_light.position = camera.viewPos;

  spot_light.direction = _calc_camera_direction();
  _light_ubo_.update();

  _light_cube_.projection() = camera.matrices.perspective;
  _light_cube_.view() = camera.matrices.view;
  _light_cube_.model() = glm::mat4{1.0f};
  _light_cube_.model() = glm::translate(_light_cube_.model(), glm::vec3{_light_ubo_.values().point_light.position});
  _light_cube_.model() = glm::scale(_light_cube_.model(), glm::vec3{0.2f});

  _light_cube_.update_uniform_buffers();
}

void vulkan_scene_renderer::prepare() {
  _get_max_usable_sample_count();
  _update_sample_count(_sample_count_, false);
  VulkanExampleBase::prepare();
  load_assets();
  _query_pool_.bind(*this);
  _light_cube_.bind(*this);
  prepare_uniform_buffers();
  setup_descriptors();
  _ts_.bind(*this);
  prepare_pipelines();
  buildCommandBuffers();
  _screenshot_.bind(*this);
  prepared = true;
}

void vulkan_scene_renderer::render() {
  draw();
  if (camera.updated) {
    update_uniform_buffers();
  }
}

void vulkan_scene_renderer::draw() {
  VulkanExampleBase::prepareFrame();
  if (resized) {
    resized = false;
    return;
  }
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &*drawCmdBuffers[currentBuffer];
  queue.submit({submitInfo}, {});

  _query_pool_.update_query_results();

  VulkanExampleBase::submitFrame();
}

void vulkan_scene_renderer::OnUpdateUIOverlay(vks::UIOverlay* overlay) {
  if (overlay->header("Camera")) {
    std::string caption = fmt::format("Position: {:.3f}, {:.3f}, {:.3f}", camera.viewPos.x, camera.viewPos.y, camera.viewPos.z);
    overlay->text(caption.c_str());

    const auto dir = _calc_camera_direction();
    caption = fmt::format("Direction: {:.3f}, {:.3f}, {:.3f}", dir.x, dir.y, dir.z);
    overlay->text(caption.c_str());
  }

  const auto& pipeline_stats = _query_pool_.query_results();
  if (!pipeline_stats.empty()) {
    if (overlay->header("Pipeline statistics")) {
      for (std::size_t i = 0; i < pipeline_stats.size(); ++i) {
        const std::string caption = fmt::format("{} : {}", _query_pool_.pipeline_stat_names()[i], pipeline_stats[i]);
        overlay->text(caption.c_str());
      }
    }
  }

  if (overlay->header("Settings")) {
    if (overlay->button("Take Screenshot")) {
      _screenshot_.capture();
    }
    if (_screenshot_.show_save_message()) {
      const auto text = fmt::format("Screenshot saved as {}", _screenshot_.filename());
      overlay->text(text.c_str());
    }

    if (overlay->checkBox("Draw Scene", &_draw_scene_)) {
      buildCommandBuffers();
    }

    if (enabledFeatures.geometryShader) {
      if (overlay->inputFloat("Scene Normals Length", &_gs_pipeline_.length(), 1.0f, 0)) {
        _gs_pipeline_.length() = std::max(_gs_pipeline_.length(), 0.0f);

        _gs_pipeline_.create_pipeline();
        buildCommandBuffers();
      }
    }

    if (deviceFeatures.fillModeNonSolid) {
      if (overlay->checkBox("Wireframe", &_wireframe_)) {
        _light_cube_.wireframe() = _wireframe_;
        _light_cube_.prepare_pipeline();

        prepare_pipelines();
        buildCommandBuffers();
      }
    }

    if (overlay->checkBox("Blinn-Phong", &_settings_ubo_.values().blinnPhong)) {
      _settings_ubo_.update();
    }

    std::vector<std::string> sample_count_labels;
    sample_count_labels.reserve(_supported_sample_counts_.size());
    for (const auto& sc : _supported_sample_counts_) {
      sample_count_labels.emplace_back(vk::to_string(sc));
    }
    if (overlay->comboBox("Multisampling", &_sample_count_option_, sample_count_labels)) {
      _update_sample_count(vk::SampleCountFlagBits{1U << static_cast<std::uint32_t>(_sample_count_option_)}, false);
      _gs_pipeline_.sample_count() = _sample_count_;
      _update_sample_count(_sample_count_);
    }
    if (enabledFeatures.sampleRateShading) {
      if (_sample_count_ != vk::SampleCountFlagBits::e1) {
        if (overlay->checkBox("Use Sample-Rate Shading", &_use_sample_shading_)) {
          _gs_pipeline_.use_sample_shading() = _use_sample_shading_;
          prepare_pipelines();
          buildCommandBuffers();
        }
      }
    } else {
      overlay->text("Sample-Rate Shading not supported.");
    }
  }

  if (overlay->header("Tessellation Shader")) {
    if (_ts_.supported()) {
      auto tess_mode_labels = std::vector<std::string>{{
          "Off",
          "Passthrough",
          "PN-Triangles"
      }};
      if (overlay->comboBox("Tessellation Mode", &_ts_.mode(), tess_mode_labels)) {
        prepare_pipelines();
        buildCommandBuffers();
      }

      if (_ts_.mode() == 2) {
        if (overlay->sliderFloat("Tessellation Alpha", &_ts_.alpha(), 0.0f, 1.0f)) {
          prepare_pipelines();
          buildCommandBuffers();
        }

        if (overlay->inputFloat("Tessellation Level", &_ts_.level(), 0.25f, 2)) {
          prepare_pipelines();
          buildCommandBuffers();
        }
      }
    } else {
      overlay->text("Tessellation Shaders not supported.");
    }
  }

  if (overlay->header("Directional Light")) {
    if (overlay->button("Reset Dir. Light")) {
      _light_ubo_.reset_dir_light();
    }

    if (overlay->sliderFloat("Dir. Light Intensity", &_light_ubo_.values().settings.dir_light_intensity, 0.0f, 1.0f)) {
      _light_ubo_.update();
    }

    auto& light = _light_ubo_.values().dir_light;
    if (overlay->sliderFloat("Dir. Light Ambient", &light.ambient, 0.0f, 1.0f)) {
      _light_ubo_.update();
    }
    if (overlay->sliderFloat("Dir. Light Diffuse", &light.diffuse, 0.0f, 1.0f)) {
      _light_ubo_.update();
    }
    if (overlay->sliderFloat("Dir. Light Specular", &light.specular, 0.0f, 1.0f)) {
      _light_ubo_.update();
    }

    if (overlay->button("Set Dir. Light Dir.")) {
      _light_ubo_.values().dir_light.direction = _calc_camera_direction();
      _light_ubo_.update();
    }
  }

  if (overlay->header("Point Light")) {
    if (overlay->checkBox("Draw Point Light", &_draw_light_)) {
      buildCommandBuffers();
    }

    if (overlay->button("Reset Point Light")) {
      _light_ubo_.reset_point_light();
    }

    if (overlay->sliderFloat("Point Light Intensity", &_light_ubo_.values().settings.point_light_intensity, 0.0f, 1.0f)) {
      _light_cube_.color() = glm::vec3{_light_ubo_.values().settings.point_light_intensity};
      _light_ubo_.update();
    }

    if (overlay->sliderInt("Point Light Distance", &_light_ubo_.point_light_distance(), 5, 100)) {
      _light_ubo_.update_distance();
    }

    auto& light = _light_ubo_.values().point_light;
    if (overlay->sliderFloat("Point Light Ambient", &light.ambient, 0.0f, 1.0f)) {
      _light_ubo_.update();
    }
    if (overlay->sliderFloat("Point Light Diffuse", &light.diffuse, 0.0f, 1.0f)) {
      _light_ubo_.update();
    }
    if (overlay->sliderFloat("Point Light Specular", &light.specular, 0.0f, 1.0f)) {
      _light_ubo_.update();
    }

    if (overlay->button("Set Point Light Pos.")) {
      _light_ubo_.values().point_light.position = camera.viewPos;
      update_uniform_buffers();
    }
  }

  if (overlay->header("Spot Light")) {
    if (overlay->button("Reset Spot Light")) {
      _light_ubo_.reset_spot_light();
    }

    if (overlay->sliderFloat("Spot Light Intensity", &_light_ubo_.values().settings.spot_light_intensity, 0.0f, 1.0f)) {
      _light_ubo_.update();
    }

    if (overlay->sliderInt("Spot Light Distance", &_light_ubo_.spot_light_distance(), 5, 100)) {
      _light_ubo_.update_distance();
    }

    auto& light = _light_ubo_.values().spot_light;
    if (overlay->sliderFloat("Spot Light Ambient", &light.ambient, 0.0f, 1.0f)) {
      _light_ubo_.update();
    }
    if (overlay->sliderFloat("Spot Light Diffuse", &light.diffuse, 0.0f, 1.0f)) {
      _light_ubo_.update();
    }
    if (overlay->sliderFloat("Spot Light Specular", &light.specular, 0.0f, 1.0f)) {
      _light_ubo_.update();
    }

    if (overlay->sliderFloat("Spot Light Inner Radius", &_light_ubo_.spot_light_inner_radius(), 0.0f, _light_ubo_.spot_light_outer_radius())) {
      _light_ubo_.update_spot_light_radius();
    }
    if (overlay->sliderFloat("Spot Light Outer Radius", &_light_ubo_.spot_light_outer_radius(), _light_ubo_.spot_light_inner_radius(), 45.0f)) {
      _light_ubo_.update_spot_light_radius();
    }
  }
}

vk::SampleCountFlagBits vulkan_scene_renderer::_get_max_usable_sample_count() {
  auto counts = std::min(deviceProperties.limits.framebufferColorSampleCounts, deviceProperties.limits.framebufferDepthSampleCounts);

  if (_supported_sample_counts_.empty()) {
    _supported_sample_counts_.emplace_back(vk::SampleCountFlagBits::e1);
    if (counts & vk::SampleCountFlagBits::e2) { _supported_sample_counts_.emplace_back(vk::SampleCountFlagBits::e2); }
    if (counts & vk::SampleCountFlagBits::e4) { _supported_sample_counts_.emplace_back(vk::SampleCountFlagBits::e4); }
    if (counts & vk::SampleCountFlagBits::e8) { _supported_sample_counts_.emplace_back(vk::SampleCountFlagBits::e8); }
    if (counts & vk::SampleCountFlagBits::e16) { _supported_sample_counts_.emplace_back(vk::SampleCountFlagBits::e16); }
    if (counts & vk::SampleCountFlagBits::e32) { _supported_sample_counts_.emplace_back(vk::SampleCountFlagBits::e32); }
    if (counts & vk::SampleCountFlagBits::e64) { _supported_sample_counts_.emplace_back(vk::SampleCountFlagBits::e64); }
  }
  return *std::max_element(_supported_sample_counts_.begin(), _supported_sample_counts_.end());
}

void vulkan_scene_renderer::_setup_multisample_target() {
  _depth_ms_target_.unbind();
  _color_ms_target_.unbind();

  _color_ms_target_.sample_count() = _sample_count_;
  _depth_ms_target_.sample_count() = _sample_count_;

  _color_ms_target_.bind(*this);
  _depth_ms_target_.bind(*this);
}

glm::vec3 vulkan_scene_renderer::_calc_camera_direction() {
  const auto flipY = camera.flipY ? -1.0f : 1.0f;

  glm::vec3 direction{0.0f};
  direction.z = -glm::cos(glm::radians(camera.rotation.y)) * glm::cos(glm::radians(camera.rotation.x * flipY));
  direction.x = glm::sin(glm::radians(camera.rotation.y)) * glm::cos(glm::radians(camera.rotation.x * flipY));
  direction.y = -glm::sin(glm::radians(camera.rotation.x * flipY));

  return direction;
}

void vulkan_scene_renderer::_update_sample_count(vk::SampleCountFlagBits sample_count, bool update_now) {
  _sample_count_ = sample_count;
  UIOverlay.rasterizationSamples = sample_count;
  _light_cube_.sample_count() = sample_count;

  if (update_now) {
    setupRenderPass();
    setupFrameBuffer();
    prepare_pipelines();
    UIOverlay.preparePipeline(*pipelineCache, *renderPass);
    _light_cube_.prepare_pipeline();

    buildCommandBuffers();
  }
}

int main(int argc, char** argv) {
  for (std::size_t i = 0; i < static_cast<std::size_t>(argc); ++i) {
    vulkan_scene_renderer::args.push_back(argv[i]);
  }
  auto app = std::make_unique<vulkan_scene_renderer>();
  app->setupWindow();
  app->initVulkan();
  app->prepare();
  app->renderLoop();
}
