#pragma once

#include "lve_device.hpp"
#include "lve_buffer.hpp"
#include "lve_descriptors.hpp"
#include "lve_pipeline.hpp"

// libs
#include <AppCore/CAPI.h>

// std
#include <memory>
#include <vector>

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
  VkRenderPass renderPass;

  // Ultralight C API
  ULRenderer renderer;
  ULView view;
  ULConfig config;

  // Vulkan Texture
  VkImage uiImage;
  VkDeviceMemory uiImageMemory;
  VkImageView uiImageView;
  VkSampler uiSampler;
  VkDescriptorSet descriptorSet;
  std::unique_ptr<LveDescriptorSetLayout> descriptorSetLayout;
  std::unique_ptr<LveDescriptorPool> descriptorPool;

  // Staging
  std::unique_ptr<LveBuffer> stagingBuffer;

  // Pipeline
  VkPipelineLayout pipelineLayout;
  std::unique_ptr<LvePipeline> lvePipeline;
};

}  // namespace lve
