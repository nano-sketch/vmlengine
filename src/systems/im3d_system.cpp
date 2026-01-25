#include "systems/im3d_system.hpp"

#include <array>
#include <cassert>
#include <stdexcept>
#include <cstring>

/**
 * im3d system implementation.
 * bridges im3d draw calls to vulkan pipelines with dynamic vertex buffers.
 */

namespace lve {

Im3dSystem::Im3dSystem(LveDevice &device, VkRenderPass rp, VkDescriptorSetLayout layout) : lveDevice{device} {
  createPipelineLayout(layout);
  createPipelines(rp);
  dynamicVertexBuffer = std::make_unique<LveBuffer>(lveDevice, sizeof(Im3dVertex), 131072, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  dynamicVertexBuffer->map();
}

Im3dSystem::~Im3dSystem() {
  vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
}

void Im3dSystem::createPipelineLayout(VkDescriptorSetLayout layout) {
  VkPipelineLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  info.setLayoutCount = 1;
  info.pSetLayouts = &layout;
  if (vkCreatePipelineLayout(lveDevice.device(), &info, nullptr, &pipelineLayout) != VK_SUCCESS) throw std::runtime_error("failed to create im3d pipeline layout");
}

void Im3dSystem::createPipelines(VkRenderPass rp) {
  PipelineConfigInfo config{};
  LvePipeline::defaultPipelineConfigInfo(config);
  config.attributeDescriptions = {{0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Im3dVertex, positionSize)}, {1, 0, VK_FORMAT_R32_UINT, offsetof(Im3dVertex, color)}};
  config.bindingDescriptions = {{0, sizeof(Im3dVertex), VK_VERTEX_INPUT_RATE_VERTEX}};
  config.depthStencilInfo.depthTestEnable = VK_FALSE;
  config.depthStencilInfo.depthWriteEnable = VK_FALSE;
  config.colorBlendAttachment.blendEnable = VK_TRUE;
  config.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  config.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  config.renderPass = rp;
  config.pipelineLayout = pipelineLayout;

  config.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
  pointsPipeline = std::make_unique<LvePipeline>(lveDevice, "shaders/im3d.vert.spv", "shaders/im3d.frag.spv", config);
  config.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
  linesPipeline = std::make_unique<LvePipeline>(lveDevice, "shaders/im3d.vert.spv", "shaders/im3d.frag.spv", config);
  config.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  trianglesPipeline = std::make_unique<LvePipeline>(lveDevice, "shaders/im3d.vert.spv", "shaders/im3d.frag.spv", config);
}

void Im3dSystem::render(FrameInfo &frameInfo) {
  uint32_t count = Im3d::GetDrawListCount();
  if (count == 0) return;

  const auto* drawLists = Im3d::GetDrawLists();
  uint32_t vPos = 0;

  for (uint32_t i = 0; i < count; ++i) {
    const auto& dl = drawLists[i];
    LvePipeline* pipeline = nullptr;
    switch (dl.m_primType) {
      case Im3d::DrawPrimitive_Points: pipeline = pointsPipeline.get(); break;
      case Im3d::DrawPrimitive_Lines: pipeline = linesPipeline.get(); break;
      case Im3d::DrawPrimitive_Triangles: pipeline = trianglesPipeline.get(); break;
      default: continue;
    }

    pipeline->bind(frameInfo.commandBuffer);
    vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);

    uint32_t vCount = dl.m_vertexCount;
    if (vCount == 0 || vPos + vCount > 131072) continue;

    VkDeviceSize off = vPos * sizeof(Im3dVertex);
    dynamicVertexBuffer->writeToBuffer((void*)dl.m_vertexData, sizeof(Im3dVertex) * vCount, off);
    VkBuffer bufs[] = {dynamicVertexBuffer->getBuffer()};
    vkCmdBindVertexBuffers(frameInfo.commandBuffer, 0, 1, bufs, &off);
    vkCmdDraw(frameInfo.commandBuffer, vCount, 1, 0, 0);
    vPos += vCount;
  }
}

Im3d::Mat4 Im3dSystem::toIm3d(const glm::mat4& m) noexcept {
  Im3d::Mat4 res;
  std::memcpy(res.m, &m[0][0], 16 * sizeof(float));
  return res;
}

}  // namespace lve
