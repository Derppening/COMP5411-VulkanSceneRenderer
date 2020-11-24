#include "light_cube.h"

light_cube::light_cube(glm::mat4 projection, glm::mat4 model, glm::mat4 view) :
    _ubo_({projection, model, view}) {}

void light_cube::setup(VulkanExampleBase& app) {
  this->_app_ = &app;

  vks::Buffer staging_vertex_buffer;
  vks::Buffer staging_index_buffer;
  _app_->vulkanDevice->createBuffer(vk::BufferUsageFlagBits::eTransferSrc,
                                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                                    &staging_vertex_buffer,
                                    sizeof(_cube_vertices),
                                    _cube_vertices.data());
  _app_->vulkanDevice->createBuffer(vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                    vk::MemoryPropertyFlagBits::eDeviceLocal,
                                    &_vertex_buffer_,
                                    sizeof(_cube_vertices));
  _app_->vulkanDevice->createBuffer(vk::BufferUsageFlagBits::eTransferSrc,
                                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                                    &staging_index_buffer,
                                    sizeof(_cube_indices),
                                    _cube_indices.data());
  _app_->vulkanDevice->createBuffer(vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                    vk::MemoryPropertyFlagBits::eDeviceLocal,
                                    &_index_buffer_,
                                    sizeof(_cube_indices));

  auto copy_cmd = _app_->vulkanDevice->createCommandBuffer(vk::CommandBufferLevel::ePrimary, true);
  vk::BufferCopy copy_region = {};

  copy_region.size = sizeof(_cube_vertices);
  copy_cmd->copyBuffer(*staging_vertex_buffer.buffer, *_vertex_buffer_.buffer, {copy_region});

  copy_region.size = sizeof(_cube_indices);
  copy_cmd->copyBuffer(*staging_index_buffer.buffer, *_index_buffer_.buffer, {copy_region});

  _app_->vulkanDevice->flushCommandBuffer(copy_cmd, _app_->queue, true);

  staging_vertex_buffer.destroy();
  staging_index_buffer.destroy();

  // Prepare uniform buffers
  _app_->vulkanDevice->createBuffer(vk::BufferUsageFlagBits::eUniformBuffer,
                                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                                    &_uniform_buffer_,
                                    sizeof(_ubo_));

  _uniform_buffer_.map();

  update_uniform_buffers();

  _setup_descriptor_set_layout();
  prepare_pipeline();
  _setup_descriptor_pool();
  _setup_descriptor_set();
}

void light_cube::destroy() {
  _pipeline_.reset();

  _pipeline_layout_.reset();
  _descriptor_set_layout_.reset();

  _uniform_buffer_.destroy();
}

void light_cube::draw(vk::CommandBuffer command_buffer) {
  command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *_pipeline_);
  command_buffer.bindVertexBuffers(0, {*_vertex_buffer_.buffer}, {0});
  command_buffer.bindIndexBuffer(*_index_buffer_.buffer, 0, vk::IndexType::eUint16);
  command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *_pipeline_layout_, 0, {_descriptor_set_}, {});
  command_buffer.pushConstants<push_consts>(*_pipeline_layout_, vk::ShaderStageFlagBits::eFragment, 0, {_push_consts_});
  command_buffer.drawIndexed(_cube_indices.size(), 1, 0, 0, 0);
}

void light_cube::_setup_descriptor_set_layout() {
  const auto set_layout_bindings = std::vector{
      // Binding 0: Vertex shader uniform buffer
      vks::initializers::descriptorSetLayoutBinding(
          vk::DescriptorType::eUniformBuffer,
          vk::ShaderStageFlagBits::eVertex,
          0)
  };

  auto descriptor_layout = vks::initializers::descriptorSetLayoutCreateInfo(set_layout_bindings);
  _descriptor_set_layout_ = _app_->device.createDescriptorSetLayoutUnique(descriptor_layout);

  vk::PushConstantRange push_constant_range;
  push_constant_range.stageFlags = vk::ShaderStageFlagBits::eFragment;
  push_constant_range.offset = 0;
  push_constant_range.size = sizeof(_push_consts_);

  auto pipeline_layout_create_info = vks::initializers::pipelineLayoutCreateInfo(&*_descriptor_set_layout_, 1);
  pipeline_layout_create_info.pushConstantRangeCount = 1;
  pipeline_layout_create_info.pPushConstantRanges = &push_constant_range;
  _pipeline_layout_ = _app_->device.createPipelineLayoutUnique(pipeline_layout_create_info);
}

void light_cube::prepare_pipeline() {
  auto input_assembly_state = vks::initializers::pipelineInputAssemblyStateCreateInfo(
      vk::PrimitiveTopology::eTriangleList,
      {},
      false);
  auto rasterization_state = vks::initializers::pipelineRasterizationStateCreateInfo(
      vk::PolygonMode::eFill,
      vk::CullModeFlagBits::eNone,
      vk::FrontFace::eCounterClockwise,
      {});
  if (_wireframe_) {
    rasterization_state.polygonMode = vk::PolygonMode::eLine;
  }
  auto blend_attachment_state = vks::initializers::pipelineColorBlendAttachmentState(
      vk::ColorComponentFlags(0xf),
      false);
  auto color_blend_state = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blend_attachment_state);
  auto depth_stencil_state = vks::initializers::pipelineDepthStencilStateCreateInfo(
      true,
      false,
      vk::CompareOp::eLessOrEqual);
  auto viewport_state = vks::initializers::pipelineViewportStateCreateInfo(1, 1, {});
  auto multisample_state = vks::initializers::pipelineMultisampleStateCreateInfo(vk::SampleCountFlagBits::e1, {});
  std::vector<vk::DynamicState> dynamic_state_enables = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
  auto dynamic_state = vks::initializers::pipelineDynamicStateCreateInfo(dynamic_state_enables);
  std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages;

  const auto vertex_input_bindings = std::vector{
      vks::initializers::vertexInputBindingDescription(0, sizeof(glm::vec3), vk::VertexInputRate::eVertex),
  };
  const auto vertex_input_attributes = std::vector{
      vks::initializers::vertexInputAttributeDescription(0,
                                                         0,
                                                         vk::Format::eR32G32B32Sfloat,
                                                         0)
  };
  vk::PipelineVertexInputStateCreateInfo vertex_input_state_ci =
      vks::initializers::pipelineVertexInputStateCreateInfo(vertex_input_bindings, vertex_input_attributes);

  auto pipeline_ci = vks::initializers::pipelineCreateInfo(*_pipeline_layout_, *_app_->renderPass, {});
  pipeline_ci.pVertexInputState = &vertex_input_state_ci;
  pipeline_ci.pInputAssemblyState = &input_assembly_state;
  pipeline_ci.pRasterizationState = &rasterization_state;
  pipeline_ci.pColorBlendState = &color_blend_state;
  pipeline_ci.pMultisampleState = &multisample_state;
  pipeline_ci.pViewportState = &viewport_state;
  pipeline_ci.pDepthStencilState = &depth_stencil_state;
  pipeline_ci.pDynamicState = &dynamic_state;
  pipeline_ci.stageCount = static_cast<uint32_t>(shader_stages.size());
  pipeline_ci.pStages = shader_stages.data();
  shader_stages[0] = _app_->loadShader(_app_->getShadersPath() + "light_cube/light_cube.vert.spv", vk::ShaderStageFlagBits::eVertex);
  shader_stages[1] = _app_->loadShader(_app_->getShadersPath() + "light_cube/light_cube.frag.spv", vk::ShaderStageFlagBits::eFragment);
  _pipeline_ = _app_->device.createGraphicsPipelineUnique(*_app_->pipelineCache, pipeline_ci).value;
}

void light_cube::_setup_descriptor_pool() {
  auto pool_sizes = std::vector{
    vks::initializers::descriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1)
  };
  auto descriptor_pool_info = vks::initializers::descriptorPoolCreateInfo(pool_sizes, _app_->swapChain.imageCount);
  _descriptor_pool_ = _app_->device.createDescriptorPoolUnique(descriptor_pool_info);
}

void light_cube::_setup_descriptor_set() {
  auto alloc_info = vks::initializers::descriptorSetAllocateInfo(*_descriptor_pool_, &*_descriptor_set_layout_, 1);
  _descriptor_set_ = _app_->device.allocateDescriptorSets(alloc_info)[0];

  const auto write_descriptor_set = vks::initializers::writeDescriptorSet(
      _descriptor_set_,
      vk::DescriptorType::eUniformBuffer,
      0,
      &_uniform_buffer_.descriptor);
  _app_->device.updateDescriptorSets({write_descriptor_set}, {});
}

void light_cube::update_uniform_buffers() {
  std::copy_n(reinterpret_cast<std::byte*>(&_ubo_), sizeof(_ubo_), static_cast<std::byte*>(_uniform_buffer_.mapped));
}
