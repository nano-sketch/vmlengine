#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE

#include "renderer/lve_gltf_model.hpp"

#include <tinygltf/json.hpp>
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>

/**
 * gltf 2.0 loader implementation.
 * parses model hierarchies and material data for modern pbr rendering.
 */

namespace lve {

LveGltfModel::LveGltfModel(LveDevice& device, const std::string& filepath) : lveDevice{device} {
  loadFromFile(filepath);
}

LveGltfModel::~LveGltfModel() = default;

void LveGltfModel::loadFromFile(const std::string& filepath) {
  tinygltf::Model input;
  tinygltf::TinyGLTF loader;
  std::string err, warn;

  bool ret = filepath.substr(filepath.find_last_of(".") + 1) == "glb" 
    ? loader.LoadBinaryFromFile(&input, &err, &warn, filepath)
    : loader.LoadASCIIFromFile(&input, &err, &warn, filepath);

  if (!warn.empty()) std::cout << "gltf warning: " << warn << std::endl;
  if (!err.empty()) std::cerr << "gltf error: " << err << std::endl;
  if (!ret) throw std::runtime_error("failed to load gltf: " + filepath);

  loadMaterials(input);
  loadNodes(input);

  vertexBuffer = std::make_unique<LveBuffer>(lveDevice, sizeof(vertices[0]), static_cast<uint32_t>(vertices.size()), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  vertexBuffer->map();
  vertexBuffer->writeToBuffer(vertices.data());

  indexBuffer = std::make_unique<LveBuffer>(lveDevice, sizeof(indices[0]), static_cast<uint32_t>(indices.size()), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  indexBuffer->map();
  indexBuffer->writeToBuffer(indices.data());
}

void LveGltfModel::loadMaterials(tinygltf::Model& input) {
  materials.reserve(input.materials.size());
  for (auto& mat : input.materials) {
    Material lveMat{};
    if (mat.values.count("baseColorFactor")) {
      lveMat.baseColorFactor = glm::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
    }
    materials.push_back(std::move(lveMat));
  }
}

void LveGltfModel::loadNodes(tinygltf::Model& input) {
  const auto& scene = input.scenes[input.defaultScene > -1 ? input.defaultScene : 0];
  nodes.reserve(scene.nodes.size());
  for (int nodeIndex : scene.nodes) {
    nodes.push_back(loadNode(nullptr, input.nodes[nodeIndex], nodeIndex, input));
  }
}

std::unique_ptr<LveGltfModel::Node> LveGltfModel::loadNode(Node* parent, const tinygltf::Node& inputNode, uint32_t nodeIndex, const tinygltf::Model& inputModel) {
  auto node = std::make_unique<Node>();
  node->parent = parent;
  if (inputNode.matrix.size() == 16) {
    node->matrix = glm::make_mat4(inputNode.matrix.data());
  } else {
    node->matrix = glm::mat4(1.0f);
  }

  if (inputNode.mesh > -1) {
    const auto& mesh = inputModel.meshes[inputNode.mesh];
    auto lveMesh = std::make_unique<Mesh>();
    for (const auto& primitive : mesh.primitives) {
      uint32_t idxStart = static_cast<uint32_t>(indices.size());
      uint32_t vStart = static_cast<uint32_t>(vertices.size());

      const auto& posAccessor = inputModel.accessors[primitive.attributes.at("POSITION")];
      const auto& posView = inputModel.bufferViews[posAccessor.bufferView];
      const float* bufferPos = reinterpret_cast<const float*>(&(inputModel.buffers[posView.buffer].data[posAccessor.byteOffset + posView.byteOffset]));
      
      for (size_t v = 0; v < posAccessor.count; v++) {
        Vertex vertex;
        vertex.pos = glm::make_vec3(&bufferPos[v * 3]);
        vertex.color = glm::vec4(1.0f);
        vertex.normal = glm::vec3(0.0f);
        vertex.uv = glm::vec2(0.0f);
        vertices.push_back(vertex);
      }

      const auto& idxAccessor = inputModel.accessors[primitive.indices];
      const auto& idxView = inputModel.bufferViews[idxAccessor.bufferView];
      const uint32_t* bufferIdx = reinterpret_cast<const uint32_t*>(&(inputModel.buffers[idxView.buffer].data[idxAccessor.byteOffset + idxView.byteOffset]));
      
      for (size_t i = 0; i < idxAccessor.count; i++) indices.push_back(bufferIdx[i] + vStart);

      Primitive prim;
      prim.firstIndex = idxStart;
      prim.indexCount = static_cast<uint32_t>(idxAccessor.count);
      prim.materialIndex = primitive.material;
      lveMesh->primitives.push_back(prim);
    }
    node->mesh = lveMesh.get();
    meshes.push_back(std::move(lveMesh));
  }

  for (int childIndex : inputNode.children) {
    node->children.push_back(loadNode(node.get(), inputModel.nodes[childIndex], childIndex, inputModel));
  }
  return node;
}

void LveGltfModel::bind(VkCommandBuffer cmd) {
  VkBuffer buffers[] = {vertexBuffer->getBuffer()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
  vkCmdBindIndexBuffer(cmd, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
}

void LveGltfModel::draw(VkCommandBuffer cmd, VkPipelineLayout layout) {
  for (const auto& node : nodes) drawNode(node.get(), cmd, layout);
}

void LveGltfModel::drawNode(Node* node, VkCommandBuffer cmd, VkPipelineLayout layout) {
  if (node->mesh) {
    for (const auto& prim : node->mesh->primitives) {
      vkCmdDrawIndexed(cmd, prim.indexCount, 1, prim.firstIndex, 0, 0);
    }
  }
  for (const auto& child : node->children) drawNode(child.get(), cmd, layout);
}

std::vector<VkVertexInputBindingDescription> LveGltfModel::Vertex::getBindingDescriptions() {
  std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
  bindingDescriptions[0].binding = 0;
  bindingDescriptions[0].stride = sizeof(Vertex);
  bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  return bindingDescriptions;
}

std::vector<VkVertexInputAttributeDescription> LveGltfModel::Vertex::getAttributeDescriptions() {
  std::vector<VkVertexInputAttributeDescription> attributeDescriptions(4);
  
  attributeDescriptions[0].binding = 0;
  attributeDescriptions[0].location = 0;
  attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[0].offset = offsetof(Vertex, pos);

  attributeDescriptions[1].binding = 0;
  attributeDescriptions[1].location = 1;
  attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
  attributeDescriptions[1].offset = offsetof(Vertex, color);

  attributeDescriptions[2].binding = 0;
  attributeDescriptions[2].location = 2;
  attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[2].offset = offsetof(Vertex, normal);

  attributeDescriptions[3].binding = 0;
  attributeDescriptions[3].location = 3;
  attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT; // Corrected from R32_SFLOAT to R32G32_SFLOAT
  attributeDescriptions[3].offset = offsetof(Vertex, uv);

  return attributeDescriptions;
}

}  // namespace lve
