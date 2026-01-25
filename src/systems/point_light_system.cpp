#include "systems/point_light_system.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <array>
#include <cassert>
#include <map>
#include <stdexcept>

/**
 * point light system implementation.
 * manages both the physical light data for shaders and the visual billboards.
 */

namespace lve {

struct PointLightPushConstants {
  glm::vec4 position{};
  glm::vec4 color{};
  float radius;
};

PointLightSystem::PointLightSystem(LveDevice& device, VkRenderPass rp, VkDescriptorSetLayout layout) : lveDevice{device} {
  createPipelineLayout(layout);
  createPipeline(rp);
}

PointLightSystem::~PointLightSystem() {
  vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
}

void PointLightSystem::createPipelineLayout(VkDescriptorSetLayout layout) {
  VkPushConstantRange range{};
  range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  range.offset = 0;
  range.size = sizeof(PointLightPushConstants);
  
  std::vector<VkDescriptorSetLayout> layouts{layout};
  VkPipelineLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  info.setLayoutCount = static_cast<uint32_t>(layouts.size());
  info.pSetLayouts = layouts.data();
  info.pushConstantRangeCount = 1;
  info.pPushConstantRanges = &range;
  
  if (vkCreatePipelineLayout(lveDevice.device(), &info, nullptr, &pipelineLayout) != VK_SUCCESS) throw std::runtime_error("failed to create pipeline layout");
}

void PointLightSystem::createPipeline(VkRenderPass rp) {
  assert(pipelineLayout != VK_NULL_HANDLE && "cannot create pipeline before layout");
  PipelineConfigInfo config{};
  LvePipeline::defaultPipelineConfigInfo(config);
  LvePipeline::enableAlphaBlending(config);
  config.attributeDescriptions.clear();
  config.bindingDescriptions.clear();
  config.renderPass = rp;
  config.pipelineLayout = pipelineLayout;
  lvePipeline = std::make_unique<LvePipeline>(lveDevice, "shaders/point_light.vert.spv", "shaders/point_light.frag.spv", config);
}

void PointLightSystem::update(FrameInfo& frameInfo, GlobalUbo& ubo) {
  auto rot = glm::rotate(glm::mat4(1.f), 0.5f * frameInfo.frameTime, {0.f, -1.f, 0.f});
  int idx = 0;
  std::vector<LveGameObject*> regular;
  LveGameObject* sun = nullptr;

  for (auto& kv : frameInfo.gameObjects) {
    auto& obj = kv.second;
    if (!obj.pointLight) continue;
    if (obj.pointLight->lightIntensity > 5000.f) sun = &obj;
    else regular.push_back(&obj);
  }

  for (auto l : regular) {
    if (idx >= MAX_LIGHTS - 1) break;
    l->transform.translation = glm::vec3(rot * glm::vec4(l->transform.translation, 1.f));
    ubo.pointLights[idx].position = glm::vec4(l->transform.translation, 1.f);
    ubo.pointLights[idx].color = glm::vec4(l->color, l->pointLight->lightIntensity);
    idx++;
  }

  if (sun) {
    ubo.pointLights[idx].position = glm::vec4(sun->transform.translation, 1.f);
    ubo.pointLights[idx].color = glm::vec4(sun->color, sun->pointLight->lightIntensity);
    idx++;
  }
  ubo.numLights = idx;
}

void PointLightSystem::render(FrameInfo& frameInfo) {
  std::map<float, LveGameObject::id_t> sorted;
  for (auto& kv : frameInfo.gameObjects) {
    auto& obj = kv.second;
    if (!obj.pointLight) continue;
    auto off = frameInfo.camera.getPosition() - obj.transform.translation;
    sorted[glm::dot(off, off)] = obj.getId();
  }

  lvePipeline->bind(frameInfo.commandBuffer);
  vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);

  for (auto it = sorted.rbegin(); it != sorted.rend(); ++it) {
    auto& obj = frameInfo.gameObjects.at(it->second);
    PointLightPushConstants push{};
    push.position = glm::vec4(obj.transform.translation, 1.f);
    push.color = glm::vec4(obj.color, obj.pointLight->lightIntensity);
    push.radius = obj.transform.scale.x;
    
    vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PointLightPushConstants), &push);
    vkCmdDraw(frameInfo.commandBuffer, 6, 1, 0, 0);
  }
}

}  // namespace lve
