/*
* Heightmap terrain generator
*
* Copyright (C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <glm/glm.hpp>
#include <glm/glm.hpp>

#include "vulkan/vulkan.h"
#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include <ktx.h>
#include <ktxvulkan.h>

namespace vks 
{
	class HeightMap
	{
	private:
		uint16_t *heightdata;
		uint32_t dim;
		uint32_t scale;

		vks::VulkanDevice *device = nullptr;
		vk::Queue copyQueue;
	public:
		enum Topology { topologyTriangles, topologyQuads };

		float heightScale = 1.0f;
		float uvScale = 1.0f;

		vks::Buffer vertexBuffer;
		vks::Buffer indexBuffer;

		struct Vertex {
			glm::vec3 pos;
			glm::vec3 normal;
			glm::vec2 uv;
		};

		size_t vertexBufferSize = 0;
		size_t indexBufferSize = 0;
		uint32_t indexCount = 0;

		HeightMap(vks::VulkanDevice *device, vk::Queue copyQueue)
		{
			this->device = device;
			this->copyQueue = copyQueue;
		};

		~HeightMap()
		{
			vertexBuffer.destroy();
			indexBuffer.destroy();
			delete[] heightdata;
		}

		float getHeight(uint32_t x, uint32_t y)
		{
			glm::ivec2 rpos = glm::ivec2(x, y) * glm::ivec2(scale);
			rpos.x = std::max(0, std::min(rpos.x, (int)dim - 1));
			rpos.y = std::max(0, std::min(rpos.y, (int)dim - 1));
			rpos /= glm::ivec2(scale);
			return *(heightdata + (rpos.x + rpos.y * dim) * scale) / 65535.0f * heightScale;
		}

		void loadFromFile(const std::string filename, uint32_t patchsize, glm::vec3 scale, Topology topology)
		{
			assert(device);
			assert(copyQueue);

			ktxResult result;
			ktxTexture* ktxTexture;
			result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
			assert(result == KTX_SUCCESS);
			ktx_size_t ktxSize = ktxTexture_GetImageSize(ktxTexture, 0);
			ktx_uint8_t* ktxImage = ktxTexture_GetData(ktxTexture);
			dim = ktxTexture->baseWidth;
			heightdata = new uint16_t[dim * dim];
			std::copy_n(reinterpret_cast<std::byte*>(ktxImage), ktxSize, reinterpret_cast<std::byte*>(heightdata));
			this->scale = dim / patchsize;
			ktxTexture_Destroy(ktxTexture);

			// Generate vertices
			Vertex * vertices = new Vertex[patchsize * patchsize * 4];

			const float wx = 2.0f;
			const float wy = 2.0f;

			for (uint32_t x = 0; x < patchsize; x++)
			{
				for (uint32_t y = 0; y < patchsize; y++)
				{
					uint32_t index = (x + y * patchsize);
					vertices[index].pos[0] = (x * wx + wx / 2.0f - (float)patchsize * wx / 2.0f) * scale.x;
					vertices[index].pos[1] = -getHeight(x, y);
					vertices[index].pos[2] = (y * wy + wy / 2.0f - (float)patchsize * wy / 2.0f) * scale.z;
					vertices[index].uv = glm::vec2((float)x / patchsize, (float)y / patchsize) * uvScale;
				}
			}

			for (uint32_t y = 0; y < patchsize; y++)
			{
				for (uint32_t x = 0; x < patchsize; x++)
				{
					float dx = getHeight(x < patchsize - 1 ? x + 1 : x, y) - getHeight(x > 0 ? x - 1 : x, y);
					if (x == 0 || x == patchsize - 1)
						dx *= 2.0f;

					float dy = getHeight(x, y < patchsize - 1 ? y + 1 : y) - getHeight(x, y > 0 ? y - 1 : y);
					if (y == 0 || y == patchsize - 1)
						dy *= 2.0f;

					glm::vec3 A = glm::vec3(1.0f, 0.0f, dx);
					glm::vec3 B = glm::vec3(0.0f, 1.0f, dy);

					glm::vec3 normal = (glm::normalize(glm::cross(A, B)) + 1.0f) * 0.5f;

					vertices[x + y * patchsize].normal = glm::vec3(normal.x, normal.z, normal.y);
				}
			}

			// Generate indices

			const uint32_t w = (patchsize - 1);
			uint32_t *indices;

			switch (topology)
			{
			// Indices for triangles
			case topologyTriangles:
			{
				indices = new uint32_t[w * w * 6];
				for (uint32_t x = 0; x < w; x++)
				{
					for (uint32_t y = 0; y < w; y++)
					{
						uint32_t index = (x + y * w) * 6;
						indices[index] = (x + y * patchsize);
						indices[index + 1] = indices[index] + patchsize;
						indices[index + 2] = indices[index + 1] + 1;

						indices[index + 3] = indices[index + 1] + 1;
						indices[index + 4] = indices[index] + 1;
						indices[index + 5] = indices[index];
					}
				}
				indexCount = (patchsize - 1) * (patchsize - 1) * 6;
				indexBufferSize = (w * w * 6) * sizeof(uint32_t);
				break;
			}
			// Indices for quad patches (tessellation)
			case topologyQuads:
			{

				indices = new uint32_t[w * w * 4];
				for (uint32_t x = 0; x < w; x++)
				{
					for (uint32_t y = 0; y < w; y++)
					{
						uint32_t index = (x + y * w) * 4;
						indices[index] = (x + y * patchsize);
						indices[index + 1] = indices[index] + patchsize;
						indices[index + 2] = indices[index + 1] + 1;
						indices[index + 3] = indices[index] + 1;
					}
				}
				indexCount = (patchsize - 1) * (patchsize - 1) * 4;
				indexBufferSize = (w * w * 4) * sizeof(uint32_t);
				break;
			}

			}

			assert(indexBufferSize > 0);

			vertexBufferSize = (patchsize * patchsize * 4) * sizeof(Vertex);

			// Generate Vulkan buffers

			vks::Buffer vertexStaging, indexStaging;

			// Create staging buffers
			device->createBuffer(
			    vk::BufferUsageFlagBits::eTransferSrc,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
				&vertexStaging,
				vertexBufferSize,
				vertices);

			device->createBuffer(
                vk::BufferUsageFlagBits::eTransferSrc,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
				&indexStaging,
				indexBufferSize,
				indices);

			// Device local (target) buffer
			device->createBuffer(
				vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
				vk::MemoryPropertyFlagBits::eDeviceLocal,
				&vertexBuffer,
				vertexBufferSize);

			device->createBuffer(
                vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                vk::MemoryPropertyFlagBits::eDeviceLocal,
				&indexBuffer,
				indexBufferSize);

			// Copy from staging buffers
			vk::UniqueCommandBuffer copyCmd = device->createCommandBuffer(vk::CommandBufferLevel::ePrimary, true);

			vk::BufferCopy copyRegion = {};

			copyRegion.size = vertexBufferSize;
			copyCmd->copyBuffer(*vertexStaging.buffer, *vertexBuffer.buffer, {copyRegion});

			copyRegion.size = indexBufferSize;
			copyCmd->copyBuffer(*indexStaging.buffer, *indexBuffer.buffer, {copyRegion});

			device->flushCommandBuffer(copyCmd, copyQueue, true);

			vertexStaging.buffer.reset();
			vertexStaging.memory.reset();
			indexStaging.buffer.reset();
			indexStaging.memory.reset();
		}
	};
}
