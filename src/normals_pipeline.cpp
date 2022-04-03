#include "normals_pipeline.h"

#include "vulkan_gltf_scene.h"

void normals_pipeline::set_pipeline_layout(vk::PipelineLayout pipeline_layout) {
  _pipeline_layout_ = pipeline_layout;
}

void normals_pipeline::create_pipeline() {
  if (!supported()) {
    return;
  }

  if (!_pipeline_layout_) {
    throw std::runtime_error("normals_pipeline::setup(): pipeline_layout not bound to instance");
  }

  auto inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(vk::PrimitiveTopology::eTriangleList, {}, false);
  auto rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise, {});
  auto blendAttachmentStateCI = vks::initializers::pipelineColorBlendAttachmentState(vk::ColorComponentFlags{0xf}, false);
  auto colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentStateCI);
  auto depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(true, false, vk::CompareOp::eLessOrEqual);
  auto viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, {});

  auto multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(_sample_count_, {});
  if (app().vulkanDevice->enabledFeatures.features.sampleRateShading && _sample_count_ != vk::SampleCountFlagBits::e1 && _use_sample_shading_) {
    multisampleStateCI.sampleShadingEnable = true;
    multisampleStateCI.minSampleShading = 0.25f;
  }

  const std::vector<vk::DynamicState> dynamicStateEnables = {
      vk::DynamicState::eViewport,
      vk::DynamicState::eScissor,
      vk::DynamicState::eLineWidth
  };
  auto dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), {});
  std::vector<vk::PipelineShaderStageCreateInfo> shaderStages{3};

  const std::vector<vk::VertexInputBindingDescription> vertexInputBindings = {
      vks::initializers::vertexInputBindingDescription(0, sizeof(vulkan_gltf_scene::vertex), vk::VertexInputRate::eVertex),
  };
  const std::vector<vk::VertexInputAttributeDescription> vertexInputAttributes = {
      vks::initializers::vertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(vulkan_gltf_scene::vertex, pos)),
      vks::initializers::vertexInputAttributeDescription(0, 1, vk::Format::eR32G32B32Sfloat, offsetof(vulkan_gltf_scene::vertex, normal)),
  };
  auto vertexInputStateCI = vks::initializers::pipelineVertexInputStateCreateInfo(vertexInputBindings, vertexInputAttributes);

  vk::GraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(_pipeline_layout_, *app().renderPass, {});
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

  if (!_shader_modules_._vert) {
    _shader_modules_._vert = app().loadShader(app().getShadersPath() + "normals/normals.vert.spv", vk::ShaderStageFlagBits::eVertex).module;
    _shader_modules_._geom = app().loadShader(app().getShadersPath() + "normals/normals.geom.spv", vk::ShaderStageFlagBits::eGeometry).module;
    _shader_modules_._frag = app().loadShader(app().getShadersPath() + "normals/normals.frag.spv", vk::ShaderStageFlagBits::eFragment).module;
  }
  shaderStages[0].stage = vk::ShaderStageFlagBits::eVertex;
  shaderStages[0].module = _shader_modules_._vert;
  shaderStages[0].pName = "main";
  shaderStages[1].stage = vk::ShaderStageFlagBits::eFragment;
  shaderStages[1].module = _shader_modules_._frag;
  shaderStages[1].pName = "main";
  shaderStages[2].stage = vk::ShaderStageFlagBits::eGeometry;
  shaderStages[2].module = _shader_modules_._geom;
  shaderStages[2].pName = "main";

  std::vector<vk::SpecializationMapEntry> gs_specialization_map_entries = {
      vks::initializers::specializationMapEntry(0, 0, sizeof(_length_))
  };
  auto gs_specialization_info = vks::initializers::specializationInfo(gs_specialization_map_entries, sizeof(_length_), &_length_);
  shaderStages[2].pSpecializationInfo = &gs_specialization_info;

  std::vector<vk::SpecializationMapEntry> vs_specialization_map_entries = {
      vks::initializers::specializationMapEntry(2, 0, sizeof(app().enabledFeatures.features.tessellationShader))
  };
  int b = true;
  auto vs_specialization_info = vks::initializers::specializationInfo(gs_specialization_map_entries, sizeof(b), &b);
  shaderStages[1].pSpecializationInfo = &vs_specialization_info;

  _pipeline_ = app().device.createGraphicsPipelineUnique(*app().pipelineCache, {pipelineCI}).value;
}

bool normals_pipeline::supported() const {
  return app().enabledFeatures.features.geometryShader;
}

void normals_pipeline::setup(VulkanExampleBase& app) {
  create_pipeline();
}

void normals_pipeline::destroy() {
  _pipeline_.reset();

  _pipeline_layout_ = nullptr;
}
