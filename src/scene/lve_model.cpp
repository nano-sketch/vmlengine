#include "scene/lve_model.hpp"
#include "core/lve_utils.hpp"
#include "renderer/lve_buffer.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <cassert>
#include <cstring>
#include <unordered_map>
#include <iostream>
#include <limits>

/**
 * model implementation.
 * handles geometry loading, vertex deduplication, and gpu buffer syncing.
 */

#ifndef ENGINE_DIR
#define ENGINE_DIR "../"
#endif

namespace std {
template <>
struct hash<lve::LveModel::Vertex> {
  size_t operator()(lve::LveModel::Vertex const &v) const {
    size_t seed = 0;
    lve::hashCombine(seed, v.position, v.color, v.normal, v.uv);
    return seed;
  }
};
}

namespace lve {

LveModel::LveModel(LveDevice &device, const LveModel::Builder &builder) : lveDevice{device} {
  createVertexBuffers(builder.vertices);
  createIndexBuffers(builder.indices);

  for (const auto& v : builder.vertices) {
    boundingBox.min = glm::min(boundingBox.min, v.position);
    boundingBox.max = glm::max(boundingBox.max, v.position);
  }

  constexpr float eps = 0.0001f;
  if (boundingBox.max.x - boundingBox.min.x < eps) { boundingBox.min.x -= eps * 0.5f; boundingBox.max.x += eps * 0.5f; }
  if (boundingBox.max.y - boundingBox.min.y < eps) { boundingBox.min.y -= eps * 0.5f; boundingBox.max.y += eps * 0.5f; }
  if (boundingBox.max.z - boundingBox.min.z < eps) { boundingBox.min.z -= eps * 0.5f; boundingBox.max.z += eps * 0.5f; }
}

LveModel::~LveModel() = default;

std::unique_ptr<LveModel> LveModel::createModelFromFile(LveDevice &device, const std::string &filepath) {
  Builder builder;
  builder.loadModel(ENGINE_DIR + filepath);
  return std::make_unique<LveModel>(device, builder);
}

void LveModel::createVertexBuffers(const std::vector<Vertex> &vertices) {
  vertexCount = static_cast<uint32_t>(vertices.size());
  assert(vertexCount >= 3 && "vertex count must be at least 3");
  VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;
  uint32_t vertexSize = sizeof(vertices[0]);

  LveBuffer staging{lveDevice, vertexSize, vertexCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
  staging.map();
  staging.writeToBuffer((void *)vertices.data());

  vertexBuffer = std::make_unique<LveBuffer>(lveDevice, vertexSize, vertexCount, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  lveDevice.copyBuffer(staging.getBuffer(), vertexBuffer->getBuffer(), bufferSize);
}

void LveModel::createIndexBuffers(const std::vector<uint32_t> &indices) {
  indexCount = static_cast<uint32_t>(indices.size());
  hasIndexBuffer = indexCount > 0;
  if (!hasIndexBuffer) return;

  VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;
  uint32_t indexSize = sizeof(indices[0]);

  LveBuffer staging{lveDevice, indexSize, indexCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
  staging.map();
  staging.writeToBuffer((void *)indices.data());

  indexBuffer = std::make_unique<LveBuffer>(lveDevice, indexSize, indexCount, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  lveDevice.copyBuffer(staging.getBuffer(), indexBuffer->getBuffer(), bufferSize);
}

void LveModel::draw(VkCommandBuffer cmd) {
  if (hasIndexBuffer) vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
  else vkCmdDraw(cmd, vertexCount, 1, 0, 0);
}

void LveModel::bind(VkCommandBuffer cmd) {
  VkBuffer buffers[] = {vertexBuffer->getBuffer()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
  if (hasIndexBuffer) vkCmdBindIndexBuffer(cmd, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
}

std::vector<VkVertexInputBindingDescription> LveModel::Vertex::getBindingDescriptions() {
  std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
  bindingDescriptions[0].binding = 0;
  bindingDescriptions[0].stride = sizeof(Vertex);
  bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  return bindingDescriptions;
}

std::vector<VkVertexInputAttributeDescription> LveModel::Vertex::getAttributeDescriptions() {
  std::vector<VkVertexInputAttributeDescription> attributeDescriptions(4);
  
  attributeDescriptions[0].binding = 0;
  attributeDescriptions[0].location = 0;
  attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[0].offset = offsetof(Vertex, position);

  attributeDescriptions[1].binding = 0;
  attributeDescriptions[1].location = 1;
  attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[1].offset = offsetof(Vertex, color);

  attributeDescriptions[2].binding = 0;
  attributeDescriptions[2].location = 2;
  attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[2].offset = offsetof(Vertex, normal);

  attributeDescriptions[3].binding = 0;
  attributeDescriptions[3].location = 3;
  attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
  attributeDescriptions[3].offset = offsetof(Vertex, uv);

  return attributeDescriptions;
}

void LveModel::Builder::loadModel(const std::string &path) {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;

  std::string baseDir = "";
  size_t lastSlash = path.find_last_of("\\/");
  if (lastSlash != std::string::npos) baseDir = path.substr(0, lastSlash + 1);

  if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str(), baseDir.c_str())) throw std::runtime_error(warn + err);

  vertices.clear();
  indices.clear();
  std::unordered_map<Vertex, uint32_t> uniqueVertices{};

  for (const auto &shape : shapes) {
    for (const auto &index : shape.mesh.indices) {
      Vertex v{};
      if (index.vertex_index >= 0) {
        v.position = {attrib.vertices[3 * index.vertex_index + 0], attrib.vertices[3 * index.vertex_index + 1], attrib.vertices[3 * index.vertex_index + 2]};
        v.color = {1.f, 1.f, 1.f};
        int matIdx = shape.mesh.material_ids[0];
        if (matIdx >= 0 && matIdx < static_cast<int>(materials.size())) {
          v.color = {materials[matIdx].diffuse[0], materials[matIdx].diffuse[1], materials[matIdx].diffuse[2]};
        } else if (!attrib.colors.empty()) {
          v.color = {attrib.colors[3 * index.vertex_index + 0], attrib.colors[3 * index.vertex_index + 1], attrib.colors[3 * index.vertex_index + 2]};
        }
      }
      if (index.normal_index >= 0) {
        v.normal = {attrib.normals[3 * index.normal_index + 0], attrib.normals[3 * index.normal_index + 1], attrib.normals[3 * index.normal_index + 2]};
      }
      if (index.texcoord_index >= 0) {
        v.uv = {attrib.texcoords[2 * index.texcoord_index + 0], attrib.texcoords[2 * index.texcoord_index + 1]};
      }
      if (uniqueVertices.find(v) == uniqueVertices.end()) {
        uniqueVertices[v] = static_cast<uint32_t>(vertices.size());
        vertices.push_back(v);
      }
      indices.push_back(uniqueVertices[v]);
    }
  }
}

}  // namespace lve
