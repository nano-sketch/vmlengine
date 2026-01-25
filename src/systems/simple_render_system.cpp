#include "systems/simple_render_system.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <array>
#include <cassert>
#include <stdexcept>

/**
 * simple render system implementation.
 * executes main forward pass with per-object textures and shadow mapping.
 */

namespace lve {

struct SimplePushConstantData {
  glm::mat4 modelMatrix{1.f};
  glm::mat4 normalMatrix{1.f};
  glm::vec2 uvScale{1.f, 1.f};
};

SimpleRenderSystem::SimpleRenderSystem(LveDevice& device, VkRenderPass rp, VkDescriptorSetLayout globalLayout) : lveDevice{device} {
  createPipelineLayout(globalLayout);
  createPipeline(rp);
}

SimpleRenderSystem::~SimpleRenderSystem() {
  vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
}

void SimpleRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalLayout) {
  VkPushConstantRange pushRange{};
  pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushRange.offset = 0;
  pushRange.size = sizeof(SimplePushConstantData);
  
  textureSetLayout = LveDescriptorSetLayout::Builder(lveDevice).addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT).build();
  shadowSetLayout = LveDescriptorSetLayout::Builder(lveDevice).addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT).build();

  std::vector<VkDescriptorSetLayout> layouts{globalLayout, textureSetLayout->getDescriptorSetLayout(), shadowSetLayout->getDescriptorSetLayout()};
  VkPipelineLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  info.setLayoutCount = static_cast<uint32_t>(layouts.size());
  info.pSetLayouts = layouts.data();
  info.pushConstantRangeCount = 1;
  info.pPushConstantRanges = &pushRange;

  if (vkCreatePipelineLayout(lveDevice.device(), &info, nullptr, &pipelineLayout) != VK_SUCCESS) throw std::runtime_error("failed to create pipeline layout");
}

void SimpleRenderSystem::createPipeline(VkRenderPass rp) {
  assert(pipelineLayout != VK_NULL_HANDLE && "cannot create pipeline before layout");
  PipelineConfigInfo config{};
  LvePipeline::defaultPipelineConfigInfo(config);
  config.renderPass = rp;
  config.pipelineLayout = pipelineLayout;
  lvePipeline = std::make_unique<LvePipeline>(lveDevice, "shaders/simple_shader.vert.spv", "shaders/simple_shader.frag.spv", config);
}

void SimpleRenderSystem::renderGameObjects(FrameInfo& frameInfo, VkDescriptorSet shadowSet) {
  lvePipeline->bind(frameInfo.commandBuffer);
  vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);
  vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &shadowSet, 0, nullptr);

  for (auto& kv : frameInfo.gameObjects) {
    auto& obj = kv.second;
    if (!obj.model) continue;

    if (obj.textureDescriptorSet != VK_NULL_HANDLE) {
      vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &obj.textureDescriptorSet, 0, nullptr);
    }

    SimplePushConstantData push{};
    push.modelMatrix = obj.transform.mat4();
    push.normalMatrix = obj.transform.normalMatrix();
    push.uvScale = obj.uvScale;
    
    vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SimplePushConstantData), &push);
    obj.model->bind(frameInfo.commandBuffer);
    obj.model->draw(frameInfo.commandBuffer);
  }
}

}  // namespace lve
