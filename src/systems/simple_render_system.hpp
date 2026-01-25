#pragma once

#include "core/lve_device.hpp"
#include "renderer/lve_frame_info.hpp"
#include "renderer/lve_pipeline.hpp"
#include "scene/lve_game_object.hpp"
#include "renderer/lve_descriptors.hpp"

#include <memory>
#include <vector>

/**
 * simple geometry rendering system.
 * manages the main forward rendering pipeline for opaque objects with textures and shadows.
 */

namespace lve {

class SimpleRenderSystem {
 public:
  SimpleRenderSystem(LveDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
  ~SimpleRenderSystem();

  SimpleRenderSystem(const SimpleRenderSystem &) = delete;
  SimpleRenderSystem &operator=(const SimpleRenderSystem &) = delete;

  LveDescriptorSetLayout& getTextureSetLayout() const noexcept { return *textureSetLayout; }
  LveDescriptorSetLayout& getShadowSetLayout() const noexcept { return *shadowSetLayout; }

  void renderGameObjects(FrameInfo &frameInfo, VkDescriptorSet shadowDescriptorSet);

 private:
  void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
  void createPipeline(VkRenderPass renderPass);

  LveDevice &lveDevice;
  std::unique_ptr<LvePipeline> lvePipeline;
  VkPipelineLayout pipelineLayout;

  std::unique_ptr<LveDescriptorSetLayout> textureSetLayout;
  std::unique_ptr<LveDescriptorSetLayout> shadowSetLayout;
};

}  // namespace lve
