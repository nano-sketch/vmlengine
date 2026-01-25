#include "renderer/lve_texture.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <cstring>

/**
 * texture implementation.
 * handles image decoding via stb_image and vulkan resource management.
 * github: https://github.com/nothings/stb
 */

namespace lve {

LveTexture::LveTexture(LveDevice &device, const std::string &filepath) : lveDevice{device} {
  int tw, th, tc;
  stbi_uc *pixels = stbi_load(filepath.c_str(), &tw, &th, &tc, STBI_rgb_alpha);
  if (!pixels) throw std::runtime_error("failed to load texture: " + filepath);
  createTexture(tw, th, pixels);
  stbi_image_free(pixels);
}

LveTexture::LveTexture(LveDevice &device, int width, int height, const unsigned char* pixels) : lveDevice{device} {
  createTexture(width, height, pixels);
}

void LveTexture::createTexture(int tw, int th, const uint8_t* pixels) {
  width = static_cast<uint32_t>(tw);
  height = static_cast<uint32_t>(th);
  mipLevels = 1;

  VkDeviceSize size = width * height * 4;
  VkBuffer staging;
  VkDeviceMemory stagingMem;
  lveDevice.createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);

  void *data;
  vkMapMemory(lveDevice.device(), stagingMem, 0, size, 0, &data);
  std::memcpy(data, pixels, size);
  vkUnmapMemory(lveDevice.device(), stagingMem);

  imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
  VkImageCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.imageType = VK_IMAGE_TYPE_2D;
  info.format = imageFormat;
  info.extent.width = width;
  info.extent.height = height;
  info.extent.depth = 1;
  info.mipLevels = 1;
  info.arrayLayers = 1;
  info.samples = VK_SAMPLE_COUNT_1_BIT;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  lveDevice.createImageWithInfo(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, imageMemory);

  transitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  copyBufferToImage(staging, width, height);
  transitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  vkDestroyBuffer(lveDevice.device(), staging, nullptr);
  vkFreeMemory(lveDevice.device(), stagingMem, nullptr);

  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = imageFormat;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;
  if (vkCreateImageView(lveDevice.device(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) throw std::runtime_error("failed to create image view");

  VkSamplerCreateInfo sampInfo{};
  sampInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampInfo.magFilter = VK_FILTER_LINEAR;
  sampInfo.minFilter = VK_FILTER_LINEAR;
  sampInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampInfo.mipLodBias = 0.0f;
  sampInfo.anisotropyEnable = VK_TRUE;
  sampInfo.maxAnisotropy = lveDevice.properties.limits.maxSamplerAnisotropy;
  sampInfo.compareEnable = VK_FALSE;
  sampInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  sampInfo.minLod = 0.0f;
  sampInfo.maxLod = 0.0f;
  sampInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  sampInfo.unnormalizedCoordinates = VK_FALSE;
  if (vkCreateSampler(lveDevice.device(), &sampInfo, nullptr, &sampler) != VK_SUCCESS) throw std::runtime_error("failed to create sampler");
}

LveTexture::~LveTexture() {
  vkDestroySampler(lveDevice.device(), sampler, nullptr);
  vkDestroyImageView(lveDevice.device(), imageView, nullptr);
  vkDestroyImage(lveDevice.device(), image, nullptr);
  vkFreeMemory(lveDevice.device(), imageMemory, nullptr);
}

void LveTexture::transitionImageLayout(VkImageLayout oldL, VkImageLayout newL) {
  auto cmd = lveDevice.beginSingleTimeCommands();
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldL;
  barrier.newLayout = newL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  VkPipelineStageFlags srcS, dstS;
  if (oldL == VK_IMAGE_LAYOUT_UNDEFINED && newL == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    srcS = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dstS = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldL == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newL == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    srcS = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dstS = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else throw std::invalid_argument("unsupported layout transition");

  vkCmdPipelineBarrier(cmd, srcS, dstS, 0, 0, nullptr, 0, nullptr, 1, &barrier);
  lveDevice.endSingleTimeCommands(cmd);
}

void LveTexture::copyBufferToImage(VkBuffer buffer, uint32_t w, uint32_t h) {
  auto cmd = lveDevice.beginSingleTimeCommands();
  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {w, h, 1};
  vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  lveDevice.endSingleTimeCommands(cmd);
}

}  // namespace lve
