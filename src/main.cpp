#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_EXTERNAL_IMAGE

#include "main.h"
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
  vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
  vkDestroyDescriptorSetLayout(device, descriptor_set_layouts.matrices, nullptr);
  vkDestroyDescriptorSetLayout(device, descriptor_set_layouts.textures, nullptr);
  shader_data.buffer.destroy();
}

void vulkan_scene_renderer::getEnabledFeatures() {
  enabledFeatures.samplerAnisotropy = deviceFeatures.samplerAnisotropy;
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
    drawCmdBuffers[i]->beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
    drawCmdBuffers[i]->setViewport(0, {viewport});
    drawCmdBuffers[i]->setScissor(0, {scissor});
    // Bind scene matrices descriptor to set 0
    drawCmdBuffers[i]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, {descriptor_set}, {});

    // POI: Draw the glTF scene
    gltf_scene.draw(*drawCmdBuffers[i], pipeline_layout);

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
    VkBuffer buffer;
    VkDeviceMemory memory;
  } vertex_staging, index_staging;

  VK_CHECK_RESULT(vulkanDevice->createBuffer(
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      vertex_buffer_size,
      &vertex_staging.buffer,
      &vertex_staging.memory,
      vertex_buffer.data()));

  // Index data
  VK_CHECK_RESULT(vulkanDevice->createBuffer(
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      index_buffer_size,
      &index_staging.buffer,
      &index_staging.memory,
      index_buffer.data()));

  // Create device local buffers (target)
  VK_CHECK_RESULT(vulkanDevice->createBuffer(
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      vertex_buffer_size,
      &gltf_scene.vertices.buffer,
      &gltf_scene.vertices.memory));
  VK_CHECK_RESULT(vulkanDevice->createBuffer(
      VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      index_buffer_size,
      &gltf_scene.indices.buffer,
      &gltf_scene.indices.memory));

  // Copy data from staging buffers (host) do device local buffer (gpu)
  VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
  VkBufferCopy copyRegion = {};

  copyRegion.size = vertex_buffer_size;
  vkCmdCopyBuffer(
      copyCmd,
      vertex_staging.buffer,
      gltf_scene.vertices.buffer,
      1,
      &copyRegion);

  copyRegion.size = index_buffer_size;
  vkCmdCopyBuffer(
      copyCmd,
      index_staging.buffer,
      gltf_scene.indices.buffer,
      1,
      &copyRegion);

  vulkanDevice->flushCommandBuffer(copyCmd, queue, true);

  // Free staging resources
  vkDestroyBuffer(device, vertex_staging.buffer, nullptr);
  vkFreeMemory(device, vertex_staging.memory, nullptr);
  vkDestroyBuffer(device, index_staging.buffer, nullptr);
  vkFreeMemory(device, index_staging.memory, nullptr);
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
  std::vector<vk::DescriptorPoolSize> pool_sizes = {
      vks::initializers::descriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1),
      vks::initializers::descriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, static_cast<uint32_t>(gltf_scene.materials.size()) * 2),
  };
  // One set for matrices and one per model image/texture
  const uint32_t max_set_count = static_cast<uint32_t>(gltf_scene.images.size()) + 1;
  vk::DescriptorPoolCreateInfo descriptor_pool_info = vks::initializers::descriptorPoolCreateInfo(pool_sizes, max_set_count);
  descriptorPool = device.createDescriptorPoolUnique(descriptor_pool_info);

  // Descriptor set layout for passing matrices
  std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
      vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0)
  };
  VkDescriptorSetLayoutCreateInfo descriptor_set_layout_ci = vks::initializers::descriptorSetLayoutCreateInfo(set_layout_bindings.data(), static_cast<uint32_t>(set_layout_bindings.size()));

  VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_ci, nullptr, &descriptor_set_layouts.matrices));

  // Descriptor set layout for passing material textures
  set_layout_bindings = {
      // Color map
      vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
      // Normal map
      vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
  };
  descriptor_set_layout_ci.pBindings = set_layout_bindings.data();
  descriptor_set_layout_ci.bindingCount = 2;
  VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_ci, nullptr, &descriptor_set_layouts.textures));

  // Pipeline layout using both descriptor sets (set 0 = matrices, set 1 = material)
  std::array<VkDescriptorSetLayout, 2> setLayouts = { descriptor_set_layouts.matrices, descriptor_set_layouts.textures };
  VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
  // We will use push constants to push the local matrices of a primitive to the vertex shader
  VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), 0);
  // Push constant ranges are part of the pipeline layout
  pipelineLayoutCI.pushConstantRangeCount = 1;
  pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
  VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipeline_layout));

  // Descriptor set for scene matrices
  VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(*descriptorPool, &descriptor_set_layouts.matrices, 1);
  VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptor_set));
  VkDescriptorBufferInfo shader_data_buffer_descriptor = shader_data.buffer.descriptor;
  VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(descriptor_set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &shader_data_buffer_descriptor);
  vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

  // Descriptor sets for materials
  for (auto& material : gltf_scene.materials) {
    const VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(*descriptorPool, &descriptor_set_layouts.textures, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &material.descriptor_set));
    VkDescriptorImageInfo colorMap = gltf_scene.get_texture_descriptor(material.base_color_texture_index);
    VkDescriptorImageInfo normalMap = gltf_scene.get_texture_descriptor(material.normal_texture_index);
    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        vks::initializers::writeDescriptorSet(material.descriptor_set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &colorMap),
        vks::initializers::writeDescriptorSet(material.descriptor_set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &normalMap),
    };
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
  }
}

void vulkan_scene_renderer::prepare_pipelines() {
  VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
  VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
  VkPipelineColorBlendAttachmentState blendAttachmentStateCI = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
  VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentStateCI);
  VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
  VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
  VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
  const std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
  VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);
  std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

  const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
      vks::initializers::vertexInputBindingDescription(0, sizeof(vulkan_gltf_scene::vertex), VK_VERTEX_INPUT_RATE_VERTEX),
  };
  const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
      vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vulkan_gltf_scene::vertex, pos)),
      vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vulkan_gltf_scene::vertex, normal)),
      vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vulkan_gltf_scene::vertex, uv)),
      vks::initializers::vertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vulkan_gltf_scene::vertex, color)),
      vks::initializers::vertexInputAttributeDescription(0, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vulkan_gltf_scene::vertex, tangent)),
  };
  VkPipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::pipelineVertexInputStateCreateInfo(vertexInputBindings, vertexInputAttributes);

  VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipeline_layout, *renderPass, 0);
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
    std::vector<VkSpecializationMapEntry> specializationMapEntries = {
        vks::initializers::specializationMapEntry(0, offsetof(MaterialSpecializationData, alphaMask), sizeof(MaterialSpecializationData::alphaMask)),
        vks::initializers::specializationMapEntry(1, offsetof(MaterialSpecializationData, alphaMaskCutoff), sizeof(MaterialSpecializationData::alphaMaskCutoff)),
    };
    VkSpecializationInfo specializationInfo = vks::initializers::specializationInfo(specializationMapEntries, sizeof(materialSpecializationData), &materialSpecializationData);
    shaderStages[1].pSpecializationInfo = &specializationInfo;

    // For double sided materials, culling will be disabled
    rasterizationStateCI.cullMode = material.double_sided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;

    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, *pipelineCache, 1, &pipelineCI, nullptr, &material.pipeline));
  }
}

void vulkan_scene_renderer::prepare_uniform_buffers() {
  VK_CHECK_RESULT(vulkanDevice->createBuffer(
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      &shader_data.buffer,
      sizeof(shader_data.values)));
  shader_data.buffer.map();
  update_uniform_buffers();
}

void vulkan_scene_renderer::update_uniform_buffers() {
  shader_data.values.projection = camera.matrices.perspective;
  shader_data.values.view = camera.matrices.view;
  shader_data.values.viewPos = camera.viewPos;
  memcpy(shader_data.buffer.mapped, &shader_data.values, sizeof(shader_data.values));
}

void vulkan_scene_renderer::prepare() {
  VulkanExampleBase::prepare();
  load_assets();
  prepare_uniform_buffers();
  setup_descriptors();
  prepare_pipelines();
  buildCommandBuffers();
  prepared = true;
}

void vulkan_scene_renderer::render() {
  renderFrame();
  if (camera.updated) {
    update_uniform_buffers();
  }
}

void vulkan_scene_renderer::OnUpdateUIOverlay(vks::UIOverlay* overlay) {
  if (overlay->header("Visibility")) {

    if (overlay->button("All")) {
      std::for_each(gltf_scene.nodes.begin(), gltf_scene.nodes.end(), [](vulkan_gltf_scene::node& node) { node.visible = true; });
      buildCommandBuffers();
    }
    ImGui::SameLine();
    if (overlay->button("None")) {
      std::for_each(gltf_scene.nodes.begin(), gltf_scene.nodes.end(), [](vulkan_gltf_scene::node& node) { node.visible = false; });
      buildCommandBuffers();
    }
    ImGui::NewLine();

    // POI: Create a list of glTF nodes for visibility toggle
    ImGui::BeginChild("#nodelist", ImVec2(200.0f, 340.0f), false);
    for (auto &node : gltf_scene.nodes) {
      if (overlay->checkBox(node.name.c_str(), &node.visible)) {
        buildCommandBuffers();
      }
    }
    ImGui::EndChild();
  }
}

int main(int argc, char** argv) {
  for (std::size_t i = 0; i < argc; ++i) {
    vulkan_scene_renderer::args.push_back(argv[i]);
  }
  auto app = std::make_unique<vulkan_scene_renderer>();
  app->setupWindow();
  app->initVulkan();
  app->prepare();
  app->renderLoop();
}
