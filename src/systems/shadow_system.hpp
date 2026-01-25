#pragma once

#include "core/lve_device.hpp"
#include "renderer/lve_frame_info.hpp"
#include "renderer/lve_pipeline.hpp"
#include "scene/lve_game_object.hpp"
#include "renderer/lve_shadow_map.hpp"

#include <memory>
#include <vector>

/**
 * shadow mapping system.
 * generates depth maps from the perspective of light sources.
 */

namespace lve {

class ShadowSystem {
 public:
  ShadowSystem(LveDevice &device, VkRenderPass renderPass);
  ~ShadowSystem();

  ShadowSystem(const ShadowSystem &) = delete;
  ShadowSystem &operator=(const ShadowSystem &) = delete;

  void renderShadowMap(FrameInfo &frameInfo, const glm::mat4 &lightProjectionView);

 private:
  void createPipelineLayout();
  void createPipeline(VkRenderPass renderPass);

  LveDevice &lveDevice;
  std::unique_ptr<LvePipeline> lvePipeline;
  VkPipelineLayout pipelineLayout;
};

}  // namespace lve
