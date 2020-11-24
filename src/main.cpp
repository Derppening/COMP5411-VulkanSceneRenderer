#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_EXTERNAL_IMAGE

#include "main.h"

#include <fmt/format.h>

constexpr bool ENABLE_VALIDATION = true;

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
  _light_cube_.destroy();
  pipeline_layout.reset();
  descriptor_set_layouts.matrices.reset();
  descriptor_set_layouts.textures.reset();
  settings_ubo.buffer.destroy();
  shader_data.buffer.destroy();
  _query_pool_.destroy();
}

void vulkan_scene_renderer::getEnabledFeatures() {
  enabledFeatures.samplerAnisotropy = deviceFeatures.samplerAnisotropy;

  if (query_pool::is_supported(deviceFeatures)) {
    enabledFeatures.pipelineStatisticsQuery = VK_TRUE;
  }
  if (deviceFeatures.fillModeNonSolid) {
    enabledFeatures.fillModeNonSolid = VK_TRUE;
  }
}

void vulkan_scene_renderer::buildCommandBuffers() {
  vk::CommandBufferBeginInfo cmd_buf_info = vks::initializers::commandBufferBeginInfo();

  std::array<vk::ClearValue, 2> clear_values;
  clear_values[0].color = defaultClearColor;
  clear_values[0].color = {std::array{0.25f, 0.25f, 0.25f, 1.0f }};
  clear_values[1].depthStencil = vk::ClearDepthStencilValue{1.0f, 0};

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

    _query_pool_.begin(*drawCmdBuffers[i]);

    // Bind scene matrices descriptor to set 0
    drawCmdBuffers[i]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline_layout, 0, {descriptor_set}, {});
    // Bind settings descriptor to set 2
    drawCmdBuffers[i]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline_layout, 2, {settings_ubo.descriptor_set}, {});

    // POI: Draw the glTF scene
    gltf_scene.draw(*drawCmdBuffers[i], *pipeline_layout);

    _light_cube_.draw(*drawCmdBuffers[i]);

    _query_pool_.end(*drawCmdBuffers[i]);

    drawUI(*drawCmdBuffers[i]);
    drawCmdBuffers[i]->endRenderPass();
    drawCmdBuffers[i]->end();
  }
}

void vulkan_scene_renderer::load_gltf_file(std::string filename) {
  tinygltf::Model gltf_input;
  tinygltf::TinyGLTF gltf_context;
  std::string error, warning;

  this->device = device;

  bool file_loaded = gltf_context.LoadASCIIFromFile(&gltf_input, &error, &warning, filename);

  // Pass some Vulkan resources required for setup and rendering to the glTF model loading class
  gltf_scene.vulkan_device = vulkanDevice.get();
  gltf_scene.copy_queue = queue;

  std::size_t pos = filename.find_last_of('/');
  gltf_scene.path = filename.substr(0, pos);

  std::vector<std::uint32_t> index_buffer;
  std::vector<vulkan_gltf_scene::vertex> vertex_buffer;

  if (file_loaded) {
    gltf_scene.load_images(gltf_input);
    gltf_scene.load_materials(gltf_input);
    gltf_scene.load_textures(gltf_input);
    const tinygltf::Scene& scene = gltf_input.scenes[0];
    for (int i : scene.nodes) {
      const tinygltf::Node node = gltf_input.nodes[static_cast<std::size_t>(i)];
      gltf_scene.load_node(node, gltf_input, nullptr, index_buffer, vertex_buffer);
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
  gltf_scene.indices.count = static_cast<int>(index_buffer.size());

  struct staging_buffer {
    vk::UniqueBuffer buffer;
    vk::UniqueDeviceMemory memory;
  } vertex_staging, index_staging;

  vulkanDevice->createBuffer(
      vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      vertex_buffer_size,
      &vertex_staging.buffer,
      &vertex_staging.memory,
      vertex_buffer.data());

  // Index data
  vulkanDevice->createBuffer(
      vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      index_buffer_size,
      &index_staging.buffer,
      &index_staging.memory,
      index_buffer.data());

  // Create device local buffers (target)
  vulkanDevice->createBuffer(
      vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eDeviceLocal,
      vertex_buffer_size,
      &gltf_scene.vertices.buffer,
      &gltf_scene.vertices.memory);
  vulkanDevice->createBuffer(
      vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eDeviceLocal,
      index_buffer_size,
      &gltf_scene.indices.buffer,
      &gltf_scene.indices.memory);

  // Copy data from staging buffers (host) do device local buffer (gpu)
  vk::UniqueCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(vk::CommandBufferLevel::ePrimary, true);
  vk::BufferCopy copyRegion = {};

  copyRegion.size = vertex_buffer_size;
  copyCmd->copyBuffer(*vertex_staging.buffer, *gltf_scene.vertices.buffer, {copyRegion});

  copyRegion.size = index_buffer_size;
  copyCmd->copyBuffer(*index_staging.buffer, *gltf_scene.indices.buffer, {copyRegion});

  vulkanDevice->flushCommandBuffer(copyCmd, queue, true);

  // Free staging resources
  vertex_staging.buffer.reset();
  vertex_staging.memory.reset();
  index_staging.buffer.reset();
  index_staging.memory.reset();
}

void vulkan_scene_renderer::load_assets() {
  load_gltf_file(getAssetPath() + "models/sponza/sponza.gltf");
}

void vulkan_scene_renderer::setup_descriptors() {
  /*
		This sample uses separate descriptor sets (and layouts) for the matrices and materials (textures)
	*/

  // One ubo to pass dynamic data to the shader
  // Two combined image samplers per material as each material uses color and normal maps
  // One more ubo to pass dynamic settings to shader
  std::vector<vk::DescriptorPoolSize> pool_sizes = {
      vks::initializers::descriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1),
      vks::initializers::descriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, static_cast<uint32_t>(gltf_scene.materials.size()) * 2),
      vks::initializers::descriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1),
  };
  // One set for matrices and one per model image/texture
  const uint32_t max_set_count = static_cast<uint32_t>(gltf_scene.images.size()) + 1;
  vk::DescriptorPoolCreateInfo descriptor_pool_info = vks::initializers::descriptorPoolCreateInfo(pool_sizes, max_set_count);
  descriptorPool = device.createDescriptorPoolUnique(descriptor_pool_info);

  // Descriptor set layout for passing matrices
  std::vector<vk::DescriptorSetLayoutBinding> set_layout_bindings = {
      vks::initializers::descriptorSetLayoutBinding(vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 0),
  };
  vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_ci = vks::initializers::descriptorSetLayoutCreateInfo(set_layout_bindings.data(), static_cast<uint32_t>(set_layout_bindings.size()));

  descriptor_set_layouts.matrices = device.createDescriptorSetLayoutUnique(descriptor_set_layout_ci);

  // Descriptor set layout for passing material textures
  set_layout_bindings = {
      // Color map
      vks::initializers::descriptorSetLayoutBinding(vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 0),
      // Normal map
      vks::initializers::descriptorSetLayoutBinding(vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1),
  };
  descriptor_set_layout_ci.pBindings = set_layout_bindings.data();
  descriptor_set_layout_ci.bindingCount = 2;
  descriptor_set_layouts.textures = device.createDescriptorSetLayoutUnique(descriptor_set_layout_ci);

  // Descriptor set layout for passing dynamic settings
  set_layout_bindings = {
      // Blinn-Phong?
      vks::initializers::descriptorSetLayoutBinding(vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0)
  };
  descriptor_set_layout_ci.setBindings(set_layout_bindings);
  settings_ubo.descriptor_set_layout = device.createDescriptorSetLayoutUnique(descriptor_set_layout_ci);

  // Pipeline layout using both descriptor sets (set 0 = matrices, set 1 = material, set 2 = settings)
  std::array<vk::DescriptorSetLayout, 3> setLayouts = {
      *descriptor_set_layouts.matrices,
      *descriptor_set_layouts.textures,
      *settings_ubo.descriptor_set_layout
  };
  vk::PipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
  // We will use push constants to push the local matrices of a primitive to the vertex shader
  vk::PushConstantRange pushConstantRange = vks::initializers::pushConstantRange(vk::ShaderStageFlagBits::eVertex, sizeof(glm::mat4), 0);
  // Push constant ranges are part of the pipeline layout
  pipelineLayoutCI.pushConstantRangeCount = 1;
  pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
  pipeline_layout = device.createPipelineLayoutUnique(pipelineLayoutCI);

  // Descriptor set for scene matrices
  vk::DescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(*descriptorPool, &*descriptor_set_layouts.matrices, 1);
  descriptor_set = device.allocateDescriptorSets(allocInfo)[0];
  vk::DescriptorBufferInfo shader_data_buffer_descriptor = shader_data.buffer.descriptor;
  vk::WriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(descriptor_set, vk::DescriptorType::eUniformBuffer, 0, &shader_data_buffer_descriptor);
  device.updateDescriptorSets({writeDescriptorSet}, {});

  // Descriptor sets for materials
  for (auto& material : gltf_scene.materials) {
    const vk::DescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(*descriptorPool, &*descriptor_set_layouts.textures, 1);
    material.descriptor_set = device.allocateDescriptorSets(allocInfo)[0];
    vk::DescriptorImageInfo colorMap = gltf_scene.get_texture_descriptor(material.base_color_texture_index);
    vk::DescriptorImageInfo normalMap = gltf_scene.get_texture_descriptor(material.normal_texture_index);
    std::vector<vk::WriteDescriptorSet> writeDescriptorSets = {
        vks::initializers::writeDescriptorSet(material.descriptor_set, vk::DescriptorType::eCombinedImageSampler, 0, &colorMap),
        vks::initializers::writeDescriptorSet(material.descriptor_set, vk::DescriptorType::eCombinedImageSampler, 1, &normalMap),
    };
    device.updateDescriptorSets(writeDescriptorSets, {});
  }

  allocInfo = vks::initializers::descriptorSetAllocateInfo(*descriptorPool, &*settings_ubo.descriptor_set_layout, 1);
  settings_ubo.descriptor_set = device.allocateDescriptorSets(allocInfo)[0];
  vk::DescriptorBufferInfo settings_ubo_buffer_descriptor = settings_ubo.buffer.descriptor;
  writeDescriptorSet = vks::initializers::writeDescriptorSet(settings_ubo.descriptor_set, vk::DescriptorType::eUniformBuffer, 0, &settings_ubo_buffer_descriptor);
  device.updateDescriptorSets({writeDescriptorSet}, {});
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
  vk::PipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(vk::SampleCountFlagBits::e1, {});
  const std::vector<vk::DynamicState> dynamicStateEnables = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
  vk::PipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), {});
  std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages;

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

  vk::GraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(*pipeline_layout, *renderPass, {});
  pipelineCI.pVertexInputState = &vertexInputStateCI;
  pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
  pipelineCI.pRasterizationState = &rasterizationStateCI;
  pipelineCI.pColorBlendState = &colorBlendStateCI;
  pipelineCI.pMultisampleState = &multisampleStateCI;
  pipelineCI.pViewportState = &viewportStateCI;
  pipelineCI.pDepthStencilState = &depthStencilStateCI;
  pipelineCI.pDynamicState = &dynamicStateCI;
  pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
  pipelineCI.pStages = shaderStages.data();

  shaderStages[0] = loadShader(getShadersPath() + "gltfscenerendering/scene.vert.spv", vk::ShaderStageFlagBits::eVertex);
  shaderStages[1] = loadShader(getShadersPath() + "gltfscenerendering/scene.frag.spv", vk::ShaderStageFlagBits::eFragment);

  // POI: Instead if using a few fixed pipelines, we create one pipeline for each material using the properties of that material
  for (auto &material : gltf_scene.materials) {

    struct MaterialSpecializationData {
      bool alphaMask;
      float alphaMaskCutoff;
    } materialSpecializationData;

    materialSpecializationData.alphaMask = material.alpha_mode == "MASK";
    materialSpecializationData.alphaMaskCutoff = material.alpha_cutoff;

    // POI: Constant fragment shader material parameters will be set using specialization constants
    std::vector<vk::SpecializationMapEntry> specializationMapEntries = {
        vks::initializers::specializationMapEntry(0, offsetof(MaterialSpecializationData, alphaMask), sizeof(MaterialSpecializationData::alphaMask)),
        vks::initializers::specializationMapEntry(1, offsetof(MaterialSpecializationData, alphaMaskCutoff), sizeof(MaterialSpecializationData::alphaMaskCutoff)),
    };
    vk::SpecializationInfo specializationInfo = vks::initializers::specializationInfo(specializationMapEntries, sizeof(materialSpecializationData), &materialSpecializationData);
    shaderStages[1].pSpecializationInfo = &specializationInfo;

    // For double sided materials, culling will be disabled
    rasterizationStateCI.cullMode = material.double_sided ? vk::CullModeFlagBits::eNone : vk::CullModeFlagBits::eBack;

    material.pipeline = device.createGraphicsPipelineUnique(*pipelineCache, {pipelineCI}).value;
  }
}

void vulkan_scene_renderer::prepare_uniform_buffers() {
  vulkanDevice->createBuffer(
      vk::BufferUsageFlagBits::eUniformBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      &shader_data.buffer,
      sizeof(shader_data.values));
  shader_data.buffer.map();
  update_uniform_buffers();

  vulkanDevice->createBuffer(
      vk::BufferUsageFlagBits::eUniformBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      &settings_ubo.buffer,
      sizeof(settings_ubo.values));
  settings_ubo.buffer.map();
  update_settings_ubo();
}

void vulkan_scene_renderer::update_uniform_buffers() {
  shader_data.values.projection = camera.matrices.perspective;
  shader_data.values.view = camera.matrices.view;
  shader_data.values.viewPos = camera.viewPos;
  memcpy(shader_data.buffer.mapped, &shader_data.values, sizeof(shader_data.values));

  _light_cube_.projection() = camera.matrices.perspective;
  _light_cube_.view() = camera.matrices.view;
  _light_cube_.model() = glm::mat4{1.0f};
  _light_cube_.model() = glm::translate(_light_cube_.model(), glm::vec3{shader_data.values.lightPos});
  _light_cube_.model() = glm::scale(_light_cube_.model(), glm::vec3{0.2f});

  _light_cube_.update_uniform_buffers();
}

void vulkan_scene_renderer::update_settings_ubo() {
  std::copy_n(reinterpret_cast<std::byte*>(&settings_ubo.values),
              sizeof(settings_ubo.values),
              static_cast<std::byte*>(settings_ubo.buffer.mapped));
}

void vulkan_scene_renderer::prepare() {
  VulkanExampleBase::prepare();
  load_assets();
  _query_pool_.setup(device, enabledFeatures);
  _light_cube_.setup(*this);
  prepare_uniform_buffers();
  setup_descriptors();
  prepare_pipelines();
  buildCommandBuffers();
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
    if (deviceFeatures.fillModeNonSolid) {
      if (overlay->checkBox("Wireframe", &_wireframe_)) {
        _light_cube_.wireframe() = _wireframe_;
        _light_cube_.prepare_pipeline();

        prepare_pipelines();
        buildCommandBuffers();
      }
    }

    if (overlay->checkBox("Blinn-Phong", &settings_ubo.values.blinnPhong)) {
      update_settings_ubo();
    }

    if (overlay->sliderFloat("Light Brightness", &settings_ubo.values.lightIntensity, 0.0f, 1.0f)) {
      update_settings_ubo();

      _light_cube_.color() = glm::vec3(settings_ubo.values.lightIntensity);
    }
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
