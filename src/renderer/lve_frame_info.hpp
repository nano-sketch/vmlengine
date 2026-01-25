#pragma once

#include "scene/lve_camera.hpp"
#include "scene/lve_game_object.hpp"

#include <vulkan/vulkan.h>

/**
 * frame metadata and per-frame uniform data.
 * defines the interface between the application and render systems.
 */

namespace lve {

static constexpr int MAX_LIGHTS = 10;

struct PointLight {
  glm::vec4 position{};
  glm::vec4 color{};
};

struct GlobalUbo {
  glm::mat4 projection{1.f};
  glm::mat4 view{1.f};
  glm::mat4 inverseView{1.f};
  glm::mat4 lightProjectionView{1.f};
  glm::vec4 ambientLightColor{1.f, 1.f, 1.f, .02f};
  PointLight pointLights[MAX_LIGHTS];
  int numLights;
};

struct FrameInfo {
  int frameIndex;
  float frameTime;
  VkCommandBuffer commandBuffer;
  LveCamera &camera;
  VkDescriptorSet globalDescriptorSet;
  LveGameObject::Map &gameObjects;
};

}  // namespace lve
