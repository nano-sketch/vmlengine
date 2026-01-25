#include "renderer/lve_shadow_map.hpp"

#include <stdexcept>

/**
 * shadow map implementation.
 * configures depth-only textures with hardware comparison for shadow filtering.
 */

namespace lve {

LveShadowMap::LveShadowMap(LveDevice &device, uint32_t w, uint32_t h)
    : lveDevice{device}, width{w}, height{h} {
  
  shadowFormat = lveDevice.findSupportedFormat(
      {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
      VK_IMAGE_TILING_OPTIMAL,
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

  VkImageCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.imageType = VK_IMAGE_TYPE_2D;
  info.format = shadowFormat;
  info.extent.width = width;
  info.extent.height = height;
  info.extent.depth = 1;
  info.mipLevels = 1;
  info.arrayLayers = 1;
  info.samples = VK_SAMPLE_COUNT_1_BIT;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  lveDevice.createImageWithInfo(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, shadowImage, shadowImageMemory);

  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = shadowImage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = shadowFormat;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(lveDevice.device(), &viewInfo, nullptr, &shadowImageView) != VK_SUCCESS) {
    throw std::runtime_error("failed to create shadow image view");
  }

  VkSamplerCreateInfo sampInfo{};
  sampInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampInfo.magFilter = VK_FILTER_LINEAR;
  sampInfo.minFilter = VK_FILTER_LINEAR;
  sampInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  sampInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  sampInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  sampInfo.mipLodBias = 0.0f;
  sampInfo.anisotropyEnable = VK_FALSE;
  sampInfo.compareEnable = VK_TRUE;
  sampInfo.compareOp = VK_COMPARE_OP_LESS;
  sampInfo.minLod = 0.0f;
  sampInfo.maxLod = 1.0f;
  sampInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

  if (vkCreateSampler(lveDevice.device(), &sampInfo, nullptr, &shadowSampler) != VK_SUCCESS) {
    throw std::runtime_error("failed to create shadow sampler");
  }
}

LveShadowMap::~LveShadowMap() {
  vkDestroySampler(lveDevice.device(), shadowSampler, nullptr);
  vkDestroyImageView(lveDevice.device(), shadowImageView, nullptr);
  vkDestroyImage(lveDevice.device(), shadowImage, nullptr);
  vkFreeMemory(lveDevice.device(), shadowImageMemory, nullptr);
}

}  // namespace lve
