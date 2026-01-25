#pragma once

#include "core/lve_device.hpp"
#include "renderer/lve_swap_chain.hpp"
#include "core/lve_window.hpp"
#include "renderer/lve_shadow_map.hpp"

#include <cassert>
#include <memory>
#include <vector>

/**
 * high level renderer class.
 * manages the swap chain lifecycle and frame synchronization.
 */

namespace lve {

class LveRenderer {
 public:
  LveRenderer(LveWindow &window, LveDevice &device);
  ~LveRenderer();

  LveRenderer(const LveRenderer &) = delete;
  LveRenderer &operator=(const LveRenderer &) = delete;

  VkRenderPass getSwapChainRenderPass() const noexcept { return lveSwapChain->getRenderPass(); }
  float getAspectRatio() const noexcept { return lveSwapChain->extentAspectRatio(); }
  bool isFrameInProgress() const noexcept { return isFrameStarted; }

  VkCommandBuffer getCurrentCommandBuffer() const {
    assert(isFrameStarted && "cannot get command buffer when frame not in progress");
    return commandBuffers[currentFrameIndex];
  }

  int getFrameIndex() const {
    assert(isFrameInProgress() && "cannot get frame index when frame not in progress");
    return currentFrameIndex;
  }

  VkExtent2D getSwapChainExtent() const noexcept { return lveSwapChain->getSwapChainExtent(); }

  VkCommandBuffer beginFrame();
  void endFrame();

  void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
  void endSwapChainRenderPass(VkCommandBuffer commandBuffer);
  
  void beginShadowRenderPass(VkCommandBuffer commandBuffer, const std::unique_ptr<LveShadowMap>& shadowMap);
  void endShadowRenderPass(VkCommandBuffer commandBuffer);

  VkRenderPass getShadowRenderPass() const noexcept { return shadowRenderPass; }

 private:
  void createCommandBuffers();
  void freeCommandBuffers();
  void recreateSwapChain();
  void createShadowRenderPass();
  void createShadowFramebuffer(const std::unique_ptr<LveShadowMap>& shadowMap);

  LveWindow &lveWindow;
  LveDevice &lveDevice;
  std::unique_ptr<LveSwapChain> lveSwapChain;
  std::vector<VkCommandBuffer> commandBuffers;

  VkRenderPass shadowRenderPass;
  VkFramebuffer shadowFramebuffer = VK_NULL_HANDLE;

  uint32_t currentImageIndex;
  int currentFrameIndex{0};
  bool isFrameStarted{false};
};
}  // namespace lve
