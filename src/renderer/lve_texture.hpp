#pragma once

#include "core/lve_device.hpp"

#include <string>

/**
 * vulkan texture representation.
 * manages image data, memory allocation, and sampling state.
 */

namespace lve {

class LveTexture {
 public:
  LveTexture(LveDevice &device, const std::string &filepath);
  LveTexture(LveDevice &device, int width, int height, const unsigned char* pixels);
  ~LveTexture();

  LveTexture(const LveTexture &) = delete;
  LveTexture &operator=(const LveTexture &) = delete;

  VkImageView getImageView() const noexcept { return imageView; }
  VkSampler getSampler() const noexcept { return sampler; }
  VkImage getImage() const noexcept { return image; }
  VkImageLayout getImageLayout() const noexcept { return imageLayout; }

 private:
  void createTexture(int width, int height, const uint8_t* pixels);
  void transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);
  void copyBufferToImage(VkBuffer buffer, uint32_t width, uint32_t height);

  LveDevice &lveDevice;
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory imageMemory = VK_NULL_HANDLE;
  VkImageView imageView = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;
  VkFormat imageFormat;
  VkImageLayout imageLayout;

  uint32_t width, height, mipLevels;
};

}  // namespace lve
