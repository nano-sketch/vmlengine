#pragma once

#include "core/lve_device.hpp"
#include "renderer/lve_buffer.hpp"
#include "renderer/lve_descriptors.hpp"
#include "renderer/lve_pipeline.hpp"

#include <AppCore/CAPI.h>

#include <memory>
#include <vector>

/**
 * html-based ui system.
 * integrates the ultralight library to provide a browser-based overlay.
 * github: https://github.com/ultralight-ux/Ultralight
 */

namespace lve {

class VlmUi {
 public:
  VlmUi(LveDevice &device, VkRenderPass renderPass, uint32_t width, uint32_t height);
  ~VlmUi();

  VlmUi(const VlmUi &) = delete;
  VlmUi &operator=(const VlmUi &) = delete;

  void update();
  void render(VkCommandBuffer commandBuffer);
  
  void handleMouseMove(double x, double y);
  void handleMouseButton(int button, int action, int mods);
  void updateTelemetry(float fps, float x, float y, float z);

  void resize(uint32_t width, uint32_t height);

 private:
  void createUiTexture();
  void updateUiTexture();
  void createPipeline(VkRenderPass renderPass);

  LveDevice &lveDevice;
  uint32_t width;
  uint32_t height;
  VkRenderPass currentRenderPass;

  ULRenderer renderer;
  ULView view;
  ULConfig config;

  VkImage uiImage = VK_NULL_HANDLE;
  VkDeviceMemory uiImageMemory = VK_NULL_HANDLE;
  VkImageView uiImageView = VK_NULL_HANDLE;
  VkSampler uiSampler = VK_NULL_HANDLE;
  VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
  
  std::unique_ptr<LveDescriptorSetLayout> descriptorSetLayout;
  std::unique_ptr<LveDescriptorPool> descriptorPool;
  std::unique_ptr<LveBuffer> stagingBuffer;

  VkPipelineLayout pipelineLayout;
  std::unique_ptr<LvePipeline> lvePipeline;
};

}  // namespace lve
