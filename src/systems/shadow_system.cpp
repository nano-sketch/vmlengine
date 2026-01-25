#include "systems/shadow_system.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <cassert>
#include <stdexcept>

/**
 * shadow system implementation.
 * configures and executes depth-only render passes for shadow map generation.
 */

namespace lve {

struct ShadowPushConstantData {
  glm::mat4 modelMatrix{1.f};
  glm::mat4 lightProjectionView{1.f};
};

ShadowSystem::ShadowSystem(LveDevice& device, VkRenderPass rp) : lveDevice{device} {
  createPipelineLayout();
  createPipeline(rp);
}

ShadowSystem::~ShadowSystem() {
  vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
}

void ShadowSystem::createPipelineLayout() {
  VkPushConstantRange range{};
  range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  range.offset = 0;
  range.size = sizeof(ShadowPushConstantData);
  
  VkPipelineLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  info.setLayoutCount = 0;
  info.pSetLayouts = nullptr;
  info.pushConstantRangeCount = 1;
  info.pPushConstantRanges = &range;
  
  if (vkCreatePipelineLayout(lveDevice.device(), &info, nullptr, &pipelineLayout) != VK_SUCCESS) throw std::runtime_error("failed to create shadow pipeline layout");
}

void ShadowSystem::createPipeline(VkRenderPass rp) {
  assert(pipelineLayout != VK_NULL_HANDLE && "cannot create pipeline before layout");
  PipelineConfigInfo config{};
  LvePipeline::defaultPipelineConfigInfo(config);
  config.attributeDescriptions = LveModel::Vertex::getAttributeDescriptions();
  config.bindingDescriptions = LveModel::Vertex::getBindingDescriptions();
  config.renderPass = rp;
  config.pipelineLayout = pipelineLayout;
  config.colorBlendInfo.attachmentCount = 0;
  config.colorBlendInfo.pAttachments = nullptr;
  lvePipeline = std::make_unique<LvePipeline>(lveDevice, "shaders/shadow.vert.spv", "shaders/shadow.frag.spv", config);
}

void ShadowSystem::renderShadowMap(FrameInfo& frameInfo, const glm::mat4& lightProjView) {
  lvePipeline->bind(frameInfo.commandBuffer);
  for (auto& kv : frameInfo.gameObjects) {
    auto& obj = kv.second;
    if (!obj.model) continue;
    ShadowPushConstantData push{};
    push.modelMatrix = obj.transform.mat4();
    push.lightProjectionView = lightProjView;
    
    vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowPushConstantData), &push);
    obj.model->bind(frameInfo.commandBuffer);
    obj.model->draw(frameInfo.commandBuffer);
  }
}

}  // namespace lve
