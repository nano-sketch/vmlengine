#pragma once

#include "core/lve_device.hpp"

#include <cstddef>

/**
 * vulkan buffer wrapper.
 * manages memory allocation, mapping, and descriptor info for gpu buffers.
 */

namespace lve {

class LveBuffer {
 public:
  LveBuffer(
      LveDevice& device,
      VkDeviceSize instanceSize,
      uint32_t instanceCount,
      VkBufferUsageFlags usageFlags,
      VkMemoryPropertyFlags memoryPropertyFlags,
      VkDeviceSize minOffsetAlignment = 1);
  ~LveBuffer();

  LveBuffer(const LveBuffer&) = delete;
  LveBuffer& operator=(const LveBuffer&) = delete;

  VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
  void unmap();

  void writeToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
  VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
  VkDescriptorBufferInfo descriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const noexcept;
  VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

  void writeToIndex(void* data, int index);
  VkResult flushIndex(int index);
  VkDescriptorBufferInfo descriptorInfoForIndex(int index) const noexcept;
  VkResult invalidateIndex(int index);

  VkBuffer getBuffer() const noexcept { return buffer; }
  void* getMappedMemory() const noexcept { return mapped; }
  uint32_t getInstanceCount() const noexcept { return instanceCount; }
  VkDeviceSize getInstanceSize() const noexcept { return instanceSize; }
  VkDeviceSize getAlignmentSize() const noexcept { return alignmentSize; }
  VkBufferUsageFlags getUsageFlags() const noexcept { return usageFlags; }
  VkMemoryPropertyFlags getMemoryPropertyFlags() const noexcept { return memoryPropertyFlags; }
  VkDeviceSize getBufferSize() const noexcept { return bufferSize; }

 private:
  static VkDeviceSize getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment) noexcept;

  LveDevice& lveDevice;
  void* mapped = nullptr;
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;

  VkDeviceSize bufferSize;
  uint32_t instanceCount;
  VkDeviceSize instanceSize;
  VkDeviceSize alignmentSize;
  VkBufferUsageFlags usageFlags;
  VkMemoryPropertyFlags memoryPropertyFlags;
};

}  // namespace lve
