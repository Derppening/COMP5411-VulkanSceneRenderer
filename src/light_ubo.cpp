#include "light_ubo.h"

const light_ubo::dir_light light_ubo::default_dir_light = {
    glm::vec3{-0.2f, -1.0f, -0.3f},
    0.05f,
    0.4f,
    0.5f
};

const light_ubo::point_light light_ubo::default_point_light = {
    glm::vec4{0.0f, 2.5f, 0.0f, 1.0f},
    1.0f,
    light_ubo::_calc_linear_term(light_ubo::_default_point_light_distance),
    light_ubo::_calc_quad_term(light_ubo::_default_point_light_distance),
    0.1f,
    1.0f,
    1.0f
};

const light_ubo::spot_light light_ubo::default_spot_light = {
    glm::vec3{},
    glm::vec3{},
    glm::cos(glm::radians(light_ubo::_default_spot_light_inner_radius)),
    glm::cos(glm::radians(light_ubo::_default_spot_light_outer_radius)),
    1.0f,
    light_ubo::_calc_linear_term(light_ubo::_default_spot_light_distance),
    light_ubo::_calc_quad_term(light_ubo::_default_spot_light_distance),
    0.0f,
    1.0f,
    1.0f
};

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
    write_descriptor_sets[i] = vks::initializers::writeDescriptorSet(_descriptor_set_,
                                                                     vk::DescriptorType::eUniformBuffer,
                                                                     static_cast<std::uint32_t>(i),
                                                                     &descriptors[i]);
  }

  device.updateDescriptorSets(write_descriptor_sets, {});
}

void light_ubo::update() {
  std::copy_n(reinterpret_cast<std::byte*>(&_values_), sizeof(_values_), static_cast<std::byte*>(_buffer_.mapped));
}

void light_ubo::update_distance(bool copy_ubo) {
  _values_.point_light.linear = _calc_linear_term(_point_light_distance_);
  _values_.point_light.quadratic = _calc_quad_term(_point_light_distance_);
  _values_.spot_light.linear = _calc_linear_term(_spot_light_distance_);
  _values_.spot_light.quadratic = _calc_quad_term(_spot_light_distance_);

  if (copy_ubo) {
    update();
  }
}

void light_ubo::update_spot_light_radius(bool copy_ubo) {
  _values_.spot_light.cutoff = glm::cos(glm::radians(_spot_light_inner_radius_));
  _values_.spot_light.outer_cutoff = glm::cos(glm::radians(_spot_light_outer_radius_));

  if (copy_ubo) {
    update();
  }
}

void light_ubo::reset_dir_light() {
  _values_.settings.dir_light_intensity = 1.0f;
  _values_.dir_light = light_ubo::default_dir_light;

  update();
}

void light_ubo::reset_point_light() {
  _values_.settings.point_light_intensity = 1.0f;
  _values_.point_light = light_ubo::default_point_light;
  _point_light_distance_ = light_ubo::_default_point_light_distance;
  update_distance();
}

void light_ubo::reset_spot_light() {
  _values_.settings.spot_light_intensity = 1.0f;

  const auto position = _values_.spot_light.position;
  const auto direction = _values_.spot_light.direction;
  _values_.spot_light = light_ubo::default_spot_light;
  _values_.spot_light.position = position;
  _values_.spot_light.direction = direction;
  _spot_light_distance_ = light_ubo::_default_point_light_distance;
  _spot_light_inner_radius_ = light_ubo::_default_spot_light_inner_radius;
  _spot_light_outer_radius_ = light_ubo::_default_spot_light_outer_radius;

  update_distance(false);
  update_spot_light_radius(false);
  update();
}

float light_ubo::_calc_linear_term(int dist) {
  return static_cast<float>(4.690508 * std::pow(dist, -1.009712));
}

float light_ubo::_calc_quad_term(int dist) {
  return static_cast<float>(82.444779 * std::pow(dist, -2.019206));
}

void light_ubo::destroy() {
  _descriptor_set_layout_.reset();
  _buffer_.destroy();
}
