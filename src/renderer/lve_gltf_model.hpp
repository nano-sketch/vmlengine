#pragma once

#include "core/lve_device.hpp"
#include "renderer/lve_buffer.hpp"
#include "renderer/lve_texture.hpp"

#include <glm/glm.hpp>
#include <tinygltf/tiny_gltf.h>

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

/**
 * gltf 2.0 model loader.
 * handles complex scene hierarchies, materials, and mesh data from gltf files.
 * github: https://github.com/syoyo/tinygltf
 */

namespace lve {

class LveGltfModel {
 public:
  struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 color;

    static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
  };

  struct Material {
    glm::vec4 baseColorFactor{1.0f};
    std::shared_ptr<LveTexture> baseColorTexture;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
  };

  struct Primitive {
    uint32_t firstIndex;
    uint32_t indexCount;
    int materialIndex;
  };

  struct Mesh {
    std::vector<Primitive> primitives;
  };

  struct Node {
    Node* parent;
    std::vector<std::unique_ptr<Node>> children;
    Mesh* mesh;
    glm::mat4 matrix;
  };

  LveGltfModel(LveDevice &device, const std::string &filepath);
  ~LveGltfModel();

  LveGltfModel(const LveGltfModel &) = delete;
  LveGltfModel& operator=(const LveGltfModel &) = delete;

  void bind(VkCommandBuffer commandBuffer);
  void draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);

 private:
  void loadFromFile(const std::string &filepath);
  void loadNodes(tinygltf::Model &input);
  void loadMaterials(tinygltf::Model &input);
  
  std::unique_ptr<Node> loadNode(Node* parent, const tinygltf::Node &inputNode, uint32_t nodeIndex, const tinygltf::Model &inputModel);
  void drawNode(Node* node, VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);

  LveDevice &lveDevice;

  std::unique_ptr<LveBuffer> vertexBuffer;
  std::unique_ptr<LveBuffer> indexBuffer;

  std::vector<uint32_t> indices;
  std::vector<Vertex> vertices;

  std::vector<std::unique_ptr<Node>> nodes;
  std::vector<Material> materials;
  std::vector<std::unique_ptr<Mesh>> meshes;
};

}  // namespace lve
