/*
 * Gizmo rendering system implementation.
 * 
 * Renders 3D axis arrows for object manipulation and orientation visualization.
 */

#include "systems/gizmo_system.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <array>
#include <cassert>
#include <stdexcept>
#include <cmath>

namespace lve {

/**
 * @brief Push constant data for gizmo rendering.
 * 
 * Contains the transformation matrix to position and scale the gizmo
 * in world space.
 */
struct GizmoPushConstantData {
  glm::mat4 modelMatrix{1.f};
  glm::vec4 color{1.f};  // RGBA color for the current axis
};

/**
 * @brief Constructor - initializes gizmo rendering resources.
 * 
 * Creates the pipeline, generates gizmo geometry (three axis arrows),
 * and uploads vertex data to GPU.
 * 
 * @param device Vulkan device handle.
 * @param renderPass Target render pass for gizmo rendering.
 * @param globalSetLayout Descriptor set layout for global UBO.
 */
GizmoSystem::GizmoSystem(LveDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
    : lveDevice{device} {
  createPipelineLayout(globalSetLayout);
  createPipeline(renderPass);
  createGizmoGeometry();
}

/**
 * @brief Destructor - releases Vulkan resources.
 */
GizmoSystem::~GizmoSystem() {
  vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
}

/**
 * @brief Creates the pipeline layout for gizmo rendering.
 * 
 * Configures push constants for model matrix and color, plus the global
 * descriptor set for camera matrices.
 * 
 * @param globalSetLayout Descriptor set layout for global UBO.
 */
void GizmoSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(GizmoPushConstantData);

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &globalSetLayout;
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

  if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create gizmo pipeline layout!");
  }
}

/**
 * @brief Creates the graphics pipeline for gizmo rendering.
 * 
 * Configures special pipeline state for gizmos:
 * - Depth testing DISABLED (gizmo always visible, even behind objects)
 * - Alpha blending enabled for smooth appearance
 * - Line width set for arrow shafts
 * 
 * @param renderPass Target render pass.
 */
void GizmoSystem::createPipeline(VkRenderPass renderPass) {
  assert(pipelineLayout != VK_NULL_HANDLE && "Cannot create pipeline before pipeline layout");

  PipelineConfigInfo pipelineConfig{};
  LvePipeline::defaultPipelineConfigInfo(pipelineConfig);
  
  // Override depth state - disable depth testing so gizmo is always visible
  pipelineConfig.depthStencilInfo.depthTestEnable = VK_FALSE;
  pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
  
  // Enable alpha blending for smooth gizmo appearance
  pipelineConfig.colorBlendAttachment.blendEnable = VK_TRUE;
  pipelineConfig.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  pipelineConfig.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  pipelineConfig.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  pipelineConfig.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  pipelineConfig.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  pipelineConfig.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
  
  pipelineConfig.renderPass = renderPass;
  pipelineConfig.pipelineLayout = pipelineLayout;
  
  lvePipeline = std::make_unique<LvePipeline>(
      lveDevice,
      "shaders/gizmo.vert.spv",
      "shaders/gizmo.frag.spv",
      pipelineConfig);
}

/**
 * @brief Generates gizmo geometry (three axis arrows).
 * 
 * Creates vertex data for three arrows pointing along X, Y, and Z axes.
 * Each arrow consists of:
 * - A line shaft from origin to tip
 * - A cone arrowhead at the tip
 * 
 * The geometry is uploaded to a GPU vertex buffer for efficient rendering.
 */
void GizmoSystem::createGizmoGeometry() {
  std::vector<LveModel::Vertex> vertices;
  
  const float arrowLength = 1.0f;
  const float arrowHeadLength = 0.2f;
  const float arrowHeadRadius = 0.1f;
  const int arrowHeadSegments = 8;
  
  // Helper lambda to create an arrow along a given axis
  auto createArrow = [&](glm::vec3 direction, glm::vec3 color) {
    glm::vec3 tip = direction * arrowLength;
    glm::vec3 headBase = direction * (arrowLength - arrowHeadLength);
    
    // Arrow shaft (line from origin to head base)
    vertices.push_back({{0.f, 0.f, 0.f}, color, direction, {0.f, 0.f}});
    vertices.push_back({headBase, color, direction, {0.f, 0.f}});
    
    // Arrow head (cone made of triangles)
    // Calculate perpendicular vectors for the cone base
    glm::vec3 perp1, perp2;
    if (std::abs(direction.y) < 0.9f) {
      perp1 = glm::normalize(glm::cross(direction, glm::vec3(0.f, 1.f, 0.f)));
    } else {
      perp1 = glm::normalize(glm::cross(direction, glm::vec3(1.f, 0.f, 0.f)));
    }
    perp2 = glm::normalize(glm::cross(direction, perp1));
    
    // Create cone triangles
    for (int i = 0; i < arrowHeadSegments; i++) {
      float angle1 = (i / static_cast<float>(arrowHeadSegments)) * glm::two_pi<float>();
      float angle2 = ((i + 1) / static_cast<float>(arrowHeadSegments)) * glm::two_pi<float>();
      
      glm::vec3 p1 = headBase + (perp1 * std::cos(angle1) + perp2 * std::sin(angle1)) * arrowHeadRadius;
      glm::vec3 p2 = headBase + (perp1 * std::cos(angle2) + perp2 * std::sin(angle2)) * arrowHeadRadius;
      
      // Triangle from base circle to tip
      vertices.push_back({p1, color, direction, {0.f, 0.f}});
      vertices.push_back({p2, color, direction, {0.f, 0.f}});
      vertices.push_back({tip, color, direction, {0.f, 0.f}});
    }
  };
  
  // Create three arrows for X, Y, Z axes
  createArrow(glm::vec3(1.f, 0.f, 0.f), glm::vec3(1.f, 0.f, 0.f));  // X-axis: Red
  createArrow(glm::vec3(0.f, -1.f, 0.f), glm::vec3(0.f, 1.f, 0.f)); // Y-axis: Green (negative Y in Vulkan)
  createArrow(glm::vec3(0.f, 0.f, 1.f), glm::vec3(0.f, 0.f, 1.f));  // Z-axis: Blue
  
  vertexCount = static_cast<uint32_t>(vertices.size());
  
  // Create vertex buffer and upload geometry
  VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;
  
  LveBuffer stagingBuffer{
      lveDevice,
      sizeof(vertices[0]),
      vertexCount,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
  
  stagingBuffer.map();
  stagingBuffer.writeToBuffer(vertices.data());
  
  vertexBuffer = std::make_unique<LveBuffer>(
      lveDevice,
      sizeof(vertices[0]),
      vertexCount,
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  
  lveDevice.copyBuffer(stagingBuffer.getBuffer(), vertexBuffer->getBuffer(), bufferSize);
}

/**
 * @brief Renders the gizmo at the specified world position.
 * 
 * Binds the gizmo pipeline and vertex buffer, then draws the gizmo geometry
 * with the appropriate transformation matrix.
 * 
 * @param frameInfo Current frame rendering context.
 * @param position World-space position for the gizmo center.
 * @param scale Size multiplier for the gizmo.
 */
void GizmoSystem::renderGizmo(FrameInfo &frameInfo, glm::vec3 position, float scale) {
  lvePipeline->bind(frameInfo.commandBuffer);
  
  // Bind global descriptor set (Set 0: camera matrices)
  vkCmdBindDescriptorSets(
      frameInfo.commandBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipelineLayout,
      0,
      1,
      &frameInfo.globalDescriptorSet,
      0,
      nullptr);
  
  // Bind gizmo vertex buffer
  VkBuffer buffers[] = {vertexBuffer->getBuffer()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(frameInfo.commandBuffer, 0, 1, buffers, offsets);
  
  // Create transformation matrix (translation + scale)
  GizmoPushConstantData push{};
  push.modelMatrix = glm::mat4(1.f);
  push.modelMatrix[3] = glm::vec4(position, 1.f);  // Set translation
  push.modelMatrix[0][0] = scale;  // Scale X
  push.modelMatrix[1][1] = scale;  // Scale Y
  push.modelMatrix[2][2] = scale;  // Scale Z
  push.color = glm::vec4(1.f);  // White (vertex colors will be used)
  
  vkCmdPushConstants(
      frameInfo.commandBuffer,
      pipelineLayout,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      0,
      sizeof(GizmoPushConstantData),
      &push);
  
  // Draw all gizmo geometry in one call
  vkCmdDraw(frameInfo.commandBuffer, vertexCount, 1, 0, 0);
}

}  // namespace lve
