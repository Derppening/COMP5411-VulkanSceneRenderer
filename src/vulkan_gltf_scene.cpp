#include "vulkan_gltf_scene.h"

#include <fmt/format.h>

vulkan_gltf_scene::~vulkan_gltf_scene() {
  // Release all Vulkan resources allocated for the model
  vertices.buffer.reset();
  vertices.memory.reset();
  indices.buffer.reset();
  indices.memory.reset();
  for (vulkan_gltf_scene::image& image : images) {
    image.texture.view.reset();
    image.texture.image.reset();
    image.texture.sampler.reset();
    image.texture.deviceMemory.reset();
  }
  for (vulkan_gltf_scene::material& material : materials) {
    vkDestroyPipeline(*vulkan_device->logicalDevice, material.pipeline, nullptr);
  }
}

void vulkan_gltf_scene::load_images(tinygltf::Model& input) {
  // POI: The textures for the glTF file used in this sample are stored as external ktx files, so we can directly load them from disk without the need for conversion
  images.resize(input.images.size());
  for (std::size_t i = 0; i < input.images.size(); ++i) {
    tinygltf::Image& gltf_image = input.images[i];
    images[i].texture.loadFromFile(path + "/" + gltf_image.uri, vk::Format::eR8G8B8A8Unorm, vulkan_device, copy_queue);
  }
}

void vulkan_gltf_scene::load_textures(tinygltf::Model& input) {
  textures.resize(input.textures.size());
  for (std::size_t i = 0; i < input.textures.size(); ++i) {
    textures[i].image_index = input.textures[i].source;
  }
}

void vulkan_gltf_scene::load_materials(tinygltf::Model& input) {
  materials.resize(input.materials.size());
  for (std::size_t i = 0; i < input.materials.size(); ++i) {
    // We only read the most basic properties required for our sample
    tinygltf::Material gltf_material = input.materials[i];
    // Get the base color factor
    if (gltf_material.values.find("baseColorFactor") != gltf_material.values.end()) {
      materials[i].base_color_factor = glm::make_vec4(gltf_material.values["baseColorFactor"].ColorFactor().data());
    }
    // Get base color texture index
    if (gltf_material.values.find("baseColorTexture") != gltf_material.values.end()) {
      materials[i].base_color_texture_index =
          static_cast<std::uint32_t>(gltf_material.values["baseColorTexture"].TextureIndex());
    }
    // Get the normal map texture index
    if (gltf_material.additionalValues.find("normalTexture") != gltf_material.additionalValues.end()) {
      materials[i].normal_texture_index =
          static_cast<std::uint32_t>(gltf_material.additionalValues["normalTexture"].TextureIndex());
    }
    // Get some additional material parameters that are used in this sample
    materials[i].alpha_mode = gltf_material.alphaMode;
    materials[i].alpha_cutoff = static_cast<float>(gltf_material.alphaCutoff);
    materials[i].double_sided = gltf_material.doubleSided;
  }
}

void vulkan_gltf_scene::load_node(const tinygltf::Node& input_node,
                                  const tinygltf::Model& input,
                                  vulkan_gltf_scene::node* parent,
                                  std::vector<std::uint32_t>& index_buffer,
                                  std::vector<vulkan_gltf_scene::vertex>& vertex_buffer) {
  vulkan_gltf_scene::node node{};
  node.name = input_node.name;

  // Get the local node matrix
  // It's either made up from translation, rotation, scale or a 4x4 matrix
  node.matrix = glm::mat4{1.0f};
  if (input_node.translation.size() == 3) {
    node.matrix = glm::translate(node.matrix, glm::vec3{glm::make_vec3(input_node.translation.data())});
  }
  if (input_node.rotation.size() == 4) {
    glm::quat q = glm::make_quat(input_node.rotation.data());
    node.matrix *= glm::mat4{q};
  }
  if (input_node.scale.size() == 3) {
    node.matrix = glm::scale(node.matrix, glm::vec3{glm::make_vec3(input_node.scale.data())});
  }
  if (input_node.matrix.size() == 16) {
    node.matrix = glm::make_mat4x4(input_node.matrix.data());
  }

  // Load node's children
  if (input_node.children.size() > 0) {
    for (size_t i = 0; i < input_node.children.size(); i++) {
      load_node(input.nodes[static_cast<std::size_t>(input_node.children[i])], input, &node, index_buffer, vertex_buffer);
    }
  }

  // If the node contains mesh data, we load vertices and indices from the buffers
  // In glTF this is done via accessors and buffer views
  if (input_node.mesh > -1) {
    const tinygltf::Mesh mesh = input.meshes[static_cast<std::size_t>(input_node.mesh)];
    // Iterate through all primitives of this node's mesh
    for (const tinygltf::Primitive& gltf_primitive : mesh.primitives) {
      auto first_index = static_cast<std::uint32_t>(index_buffer.size());
      auto vertex_start = static_cast<std::uint32_t>(vertex_buffer.size());
      std::uint32_t index_count = 0;

      // Vertices
      {
        const float* position_buffer = nullptr;
        const float* normals_buffer = nullptr;
        const float* tex_coords_buffer = nullptr;
        const float* tangents_buffer = nullptr;
        std::size_t vertex_count = 0;

        // Get buffer data for vertex position
        if (gltf_primitive.attributes.find("POSITION") != gltf_primitive.attributes.end()) {
          const tinygltf::Accessor& accessor =
              input.accessors[static_cast<std::size_t>(gltf_primitive.attributes.find("POSITION")->second)];
          const tinygltf::BufferView& view = input.bufferViews[static_cast<std::size_t>(accessor.bufferView)];
          position_buffer = reinterpret_cast<const float*>(&(input.buffers[static_cast<std::size_t>(view.buffer)].data[
              accessor.byteOffset + view.byteOffset]));
          vertex_count = accessor.count;
        }
        // Get buffer data for vertex normals
        if (gltf_primitive.attributes.find("NORMAL") != gltf_primitive.attributes.end()) {
          const tinygltf::Accessor
              & accessor = input.accessors[static_cast<std::size_t>(gltf_primitive.attributes.find("NORMAL")->second)];
          const tinygltf::BufferView& view = input.bufferViews[static_cast<std::size_t>(accessor.bufferView)];
          normals_buffer = reinterpret_cast<const float*>(&(input.buffers[static_cast<std::size_t>(view.buffer)].data[
              accessor.byteOffset + view.byteOffset]));
        }
        // Get buffer data for vertex texture coordinates
        // glTF supports multiple sets, we only load the first one
        if (gltf_primitive.attributes.find("TEXCOORD_0") != gltf_primitive.attributes.end()) {
          const tinygltf::Accessor& accessor =
              input.accessors[static_cast<std::size_t>(gltf_primitive.attributes.find("TEXCOORD_0")->second)];
          const tinygltf::BufferView& view = input.bufferViews[static_cast<std::size_t>(accessor.bufferView)];
          tex_coords_buffer =
              reinterpret_cast<const float*>(&(input.buffers[static_cast<std::size_t>(view.buffer)].data[
                  accessor.byteOffset + view.byteOffset]));
        }
        // POI: This sample uses normal mapping, so we also need to load the tangents from the glTF file
        if (gltf_primitive.attributes.find("TANGENT") != gltf_primitive.attributes.end()) {
          const tinygltf::Accessor
              & accessor = input.accessors[static_cast<std::size_t>(gltf_primitive.attributes.find("TANGENT")->second)];
          const tinygltf::BufferView& view = input.bufferViews[static_cast<std::size_t>(accessor.bufferView)];
          tangents_buffer = reinterpret_cast<const float*>(&(input.buffers[static_cast<std::size_t>(view.buffer)].data[
              accessor.byteOffset + view.byteOffset]));
        }

        // Append data to model's vertex buffer
        for (size_t v = 0; v < vertex_count; v++) {
          vulkan_gltf_scene::vertex vert{};
          vert.pos = glm::vec4(glm::make_vec3(&position_buffer[v * 3]), 1.0f);
          vert.normal =
              glm::normalize(glm::vec3{normals_buffer ? glm::make_vec3(&normals_buffer[v * 3]) : glm::vec3{0.0f}});
          vert.uv = tex_coords_buffer ? glm::make_vec2(&tex_coords_buffer[v * 2]) : glm::vec3{0.0f};
          vert.color = glm::vec3{1.0f};
          vert.tangent = tangents_buffer ? glm::make_vec4(&tangents_buffer[v * 4]) : glm::vec4{0.0f};
          vertex_buffer.emplace_back(vert);
        }
      }
      // Indices
      {
        const tinygltf::Accessor& accessor = input.accessors[static_cast<std::size_t>(gltf_primitive.indices)];
        const tinygltf::BufferView& bufferView = input.bufferViews[static_cast<std::size_t>(accessor.bufferView)];
        const tinygltf::Buffer& buffer = input.buffers[static_cast<std::size_t>(bufferView.buffer)];

        index_count += static_cast<std::uint32_t>(accessor.count);

        // glTF supports different component types of indices
        switch (accessor.componentType) {
          case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
            auto buf = std::make_unique<std::uint32_t[]>(accessor.count);
            std::copy_n(reinterpret_cast<const std::byte*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]),
                        accessor.count * sizeof(std::uint32_t),
                        reinterpret_cast<std::byte*>(buf.get()));
            for (std::size_t index = 0; index < accessor.count; ++index) {
              index_buffer.push_back(buf[index] + vertex_start);
            }
            break;
          }
          case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
            auto buf = std::make_unique<std::uint16_t[]>(accessor.count);
            std::copy_n(reinterpret_cast<const std::byte*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]),
                        accessor.count * sizeof(std::uint16_t),
                        reinterpret_cast<std::byte*>(buf.get()));
            for (size_t index = 0; index < accessor.count; index++) {
              index_buffer.push_back(buf[index] + vertex_start);
            }
            break;
          }
          case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
            auto buf = std::make_unique<std::uint8_t[]>(accessor.count);
            std::copy_n(reinterpret_cast<const std::byte*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]),
                        accessor.count * sizeof(std::uint8_t),
                        reinterpret_cast<std::byte*>(buf.get()));
            for (size_t index = 0; index < accessor.count; index++) {
              index_buffer.push_back(buf[index] + vertex_start);
            }
            break;
          }
          default:
            fmt::print(stderr, "Index component type {} not supported!\n", accessor.componentType);
            std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
            return;
        }
      }
      vulkan_gltf_scene::primitive primitive{};
      primitive.first_index = first_index;
      primitive.index_count = index_count;
      primitive.material_index = gltf_primitive.material;
      node.mesh.primitives.emplace_back(primitive);
    }
  }

  if (parent) {
    parent->children.push_back(node);
  } else {
    nodes.push_back(node);
  }
}

vk::DescriptorImageInfo vulkan_gltf_scene::get_texture_descriptor(std::size_t index) {
  return images[index].texture.descriptor;
}

void vulkan_gltf_scene::draw_node(vk::CommandBuffer command_buffer,
                                  vk::PipelineLayout pipeline_layout,
                                  const vulkan_gltf_scene::node& node) {
  if (!node.visible) {
    return;
  }
  if (!node.mesh.primitives.empty()) {
    // Pass the node's matrix via push constants
    // Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
    glm::mat4 node_matrix = node.matrix;
    vulkan_gltf_scene::node* current_parent = node.parent;
    while (current_parent) {
      node_matrix = current_parent->matrix * node_matrix;
      current_parent = current_parent->parent;
    }
    // Pass the final matrix to the vertex shader using push constants
    vkCmdPushConstants(command_buffer,pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &node_matrix);
    for (const vulkan_gltf_scene::primitive& primitive : node.mesh.primitives) {
      if (primitive.index_count > 0) {
        vulkan_gltf_scene::material& material = materials[static_cast<std::size_t>(primitive.material_index)];
        // POI: Bind the pipeline for the node's material
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline);
        command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 1, {material.descriptor_set}, {});
        vkCmdDrawIndexed(command_buffer, primitive.index_count, 1, primitive.first_index, 0, 0);
      }
    }
  }
  for (auto& child : node.children) {
    draw_node(command_buffer, pipeline_layout, child);
  }
}

void vulkan_gltf_scene::draw(vk::CommandBuffer command_buffer, vk::PipelineLayout pipeline_layout) {
  // All vertices and indices are stored in single buffers, so we only need to bind once
  auto offsets = std::array<vk::DeviceSize, 1>{{0}};
  command_buffer.bindVertexBuffers(0, {*vertices.buffer}, offsets);
  command_buffer.bindIndexBuffer(*indices.buffer, 0, vk::IndexType::eUint32);
  // Render all nodes at top-level
  for (auto& node : nodes) {
    draw_node(command_buffer, pipeline_layout, node);
  }
}
