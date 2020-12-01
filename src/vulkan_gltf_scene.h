#pragma once

#include <tiny_gltf.h>
#include <vulkan/vulkan.hpp>

#include "vulkanexamplebase.h"

class vulkan_gltf_scene {
 public:
  vks::VulkanDevice* vulkan_device;
  vk::Queue copy_queue;

  struct vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec3 color;
    glm::vec4 tangent;
  };

  vks::Buffer vertices;

  struct {
    int count;
    vks::Buffer buffer;
  } indices;

  struct node;

  struct primitive {
    std::uint32_t first_index;
    std::uint32_t index_count;
    std::int32_t material_index;
  };

  struct mesh {
    std::vector<primitive> primitives;
  };

  struct node {
    node* parent;
    std::vector<node> children;
    vulkan_gltf_scene::mesh mesh;
    glm::mat4 matrix;
    std::string name;
    bool visible = true;
  };

  struct material {
    glm::vec4 base_color_factor = glm::vec4{1.0f};
    std::uint32_t base_color_texture_index;
    std::uint32_t normal_texture_index;
    std::string alpha_mode = "OPAQUE";
    float alpha_cutoff;
    bool double_sided = false;
    vk::DescriptorSet descriptor_set;
    vk::UniquePipeline pipeline;
  };

  struct image {
    vks::Texture2D texture;
  };

  struct texture {
    std::int32_t image_index;
  };

  std::vector<image> images;
  std::vector<texture> textures;
  std::vector<material> materials;
  std::vector<node> nodes;

  std::string path;

  ~vulkan_gltf_scene();
  vk::DescriptorImageInfo get_texture_descriptor(std::size_t index);
  void load_images(tinygltf::Model& input);
  void load_textures(tinygltf::Model& input);
  void load_materials(tinygltf::Model& input);
  void load_node(const tinygltf::Node& input_node,
                 const tinygltf::Model& input,
                 vulkan_gltf_scene::node* parent,
                 std::vector<std::uint32_t>& index_buffer,
                 std::vector<vulkan_gltf_scene::vertex>& vertex_buffer);
  void draw_node(vk::CommandBuffer command_buffer,
                 vk::PipelineLayout pipeline_layout,
                 const vulkan_gltf_scene::node& node,
                 vk::Pipeline pipeline = {});
  void draw(vk::CommandBuffer command_buffer, vk::PipelineLayout pipeline_layout, vk::Pipeline pipeline = {});
};
