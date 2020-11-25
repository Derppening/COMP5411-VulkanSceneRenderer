#include "light_ubo.h"

void light_ubo::prepare(vks::VulkanDevice& vulkan_device, bool update_now) {
  vulkan_device.createBuffer(
      vk::BufferUsageFlagBits::eUniformBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      &_buffer_,
      sizeof(_values_));
  _buffer_.map();

  if (update_now) {
    update();
  }
}

void light_ubo::setup_descriptor_set_layout(vk::Device device, vk::ShaderStageFlags stage_flags) {
  auto set_layout_bindings = std::vector{
      vks::initializers::descriptorSetLayoutBinding(vk::DescriptorType::eUniformBuffer, stage_flags, 0),
      vks::initializers::descriptorSetLayoutBinding(vk::DescriptorType::eUniformBuffer, stage_flags, 1),
      vks::initializers::descriptorSetLayoutBinding(vk::DescriptorType::eUniformBuffer, stage_flags, 2),
      vks::initializers::descriptorSetLayoutBinding(vk::DescriptorType::eUniformBuffer, stage_flags, 3)
  };
  auto descriptor_set_layout_ci = vk::DescriptorSetLayoutCreateInfo{}
      .setBindings(set_layout_bindings);

  _descriptor_set_layout_ = device.createDescriptorSetLayoutUnique(descriptor_set_layout_ci);
}

void light_ubo::setup_descriptor_sets(vk::Device device, vk::DescriptorPool descriptor_pool) {
  auto alloc_info = vks::initializers::descriptorSetAllocateInfo(descriptor_pool, &*_descriptor_set_layout_, 1);
  _descriptor_set_ = device.allocateDescriptorSets(alloc_info)[0];

  auto range = std::vector{
      sizeof(settings),
      sizeof(dir_light),
      sizeof(point_light),
      sizeof(spot_light)
  };
  auto offsets = std::vector{
      offsetof(struct values, settings),
      offsetof(struct values, dir_light),
      offsetof(struct values, point_light),
      offsetof(struct values, spot_light)
  };
  std::vector<vk::DescriptorBufferInfo> descriptors{4};
  for (std::size_t i = 0; i < descriptors.size(); ++i) {
    descriptors[i] = _buffer_.descriptor
        .setOffset(offsets[i])
        .setRange(range[i]);
  }

  std::vector<vk::WriteDescriptorSet> write_descriptor_sets{4};
  for (std::size_t i = 0; i < write_descriptor_sets.size(); ++i) {
    write_descriptor_sets[i] =
        vks::initializers::writeDescriptorSet(_descriptor_set_, vk::DescriptorType::eUniformBuffer, i, &descriptors[i]);
  }

  device.updateDescriptorSets(write_descriptor_sets, {});
}

void light_ubo::update() {
  std::copy_n(reinterpret_cast<std::byte*>(&_values_), sizeof(_values_), static_cast<std::byte*>(_buffer_.mapped));
}

void light_ubo::destroy() {
  _descriptor_set_layout_.reset();
  _buffer_.destroy();
}
