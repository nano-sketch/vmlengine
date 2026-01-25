#pragma once

#include "core/lve_device.hpp"

/**
 * shadow map resource management.
 * handles creation of depth attachments and samplers for shadow logic.
 */

namespace lve {

class LveShadowMap {
 public:
  LveShadowMap(LveDevice &device, uint32_t width, uint32_t height);
  ~LveShadowMap();

  LveShadowMap(const LveShadowMap &) = delete;
  LveShadowMap &operator=(const LveShadowMap &) = delete;

  VkImageView getShadowImageView() const noexcept { return shadowImageView; }
  VkSampler getShadowSampler() const noexcept { return shadowSampler; }
  VkImage getShadowImage() const noexcept { return shadowImage; }
  VkFormat getShadowFormat() const noexcept { return shadowFormat; }
  uint32_t getWidth() const noexcept { return width; }
  uint32_t getHeight() const noexcept { return height; }

 private:
  LveDevice &lveDevice;
  
  VkImage shadowImage = VK_NULL_HANDLE;
  VkDeviceMemory shadowImageMemory = VK_NULL_HANDLE;
  VkImageView shadowImageView = VK_NULL_HANDLE;
  VkSampler shadowSampler = VK_NULL_HANDLE;
  VkFormat shadowFormat;

  uint32_t width, height;
};

}  // namespace lve
