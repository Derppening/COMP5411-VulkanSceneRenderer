#pragma once

#include <algorithm>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <VulkanBuffer.h>
#include <VulkanDevice.h>

template<typename T>
class ubo {
 public:
  ubo() = default;
  explicit ubo(const T& other);
  explicit ubo(T&& other) noexcept;

  void prepare(vks::VulkanDevice& vulkan_device, bool update_now = true);
  void destroy();

  void setup_descriptor_set_layout(vk::Device device, vk::ShaderStageFlags stage_flags);
  void setup_descriptor_sets(vk::Device device, vk::DescriptorPool descriptor_pool);

  void update();

  T& values() { return _values_; }
  vk::DescriptorSetLayout descriptor_set_layout() const { return *_descriptor_set_layout_; }
  vk::DescriptorSet descriptor_set() const { return _descriptor_set_; }

 private:
  vks::Buffer _buffer_;
  T _values_;
  vk::UniqueDescriptorSetLayout _descriptor_set_layout_;
  vk::DescriptorSet _descriptor_set_;
};

template<typename T>
ubo<T>::ubo(const T& other) : _values_(other) {}

template<typename T>
ubo<T>::ubo(T&& other) noexcept : _values_(std::move(other)) {}

template<typename T>
void ubo<T>::prepare(vks::VulkanDevice& vulkan_device, bool update_now) {
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

template<typename T>
void ubo<T>::setup_descriptor_set_layout(vk::Device device,
                                 vk::ShaderStageFlags stage_flags) {
  auto set_layout_bindings = std::vector{
      vks::initializers::descriptorSetLayoutBinding(vk::DescriptorType::eUniformBuffer, stage_flags, 0)
  };
  auto descriptor_set_layout_ci = vk::DescriptorSetLayoutCreateInfo{}
      .setBindings(set_layout_bindings);

  _descriptor_set_layout_ = device.createDescriptorSetLayoutUnique(descriptor_set_layout_ci);
}

template<typename T>
void ubo<T>::setup_descriptor_sets(vk::Device device, vk::DescriptorPool descriptor_pool) {
  auto alloc_info = vks::initializers::descriptorSetAllocateInfo(descriptor_pool, &*_descriptor_set_layout_, 1);
  _descriptor_set_ = device.allocateDescriptorSets(alloc_info)[0];
  auto write_descriptor_set = vks::initializers::writeDescriptorSet(_descriptor_set_,
                                                                    vk::DescriptorType::eUniformBuffer,
                                                                    0,
                                                                    &_buffer_.descriptor);
  device.updateDescriptorSets({write_descriptor_set}, {});
}

template<typename T>
void ubo<T>::update() {
  std::copy_n(reinterpret_cast<std::byte*>(&_values_), sizeof(_values_), static_cast<std::byte*>(_buffer_.mapped));
}

template<typename T>
void ubo<T>::destroy() {
  _descriptor_set_layout_.reset();
  _buffer_.destroy();
}
