#include "renderer/lve_buffer.hpp"

#include <cassert>
#include <cstring>
#include <algorithm>
#include <iterator>

/**
 * buffer implementation.
 * manages memory alignment and host-to-device data transfers.
 * based on: https://github.com/SaschaWillems/Vulkan/blob/master/base/VulkanBuffer.h
 */

namespace lve {

VkDeviceSize LveBuffer::getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment) noexcept {
  return minOffsetAlignment > 0 ? (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1) : instanceSize;
}

LveBuffer::LveBuffer(
    LveDevice &device,
    VkDeviceSize instanceSize,
    uint32_t instanceCount,
    VkBufferUsageFlags usageFlags,
    VkMemoryPropertyFlags memoryPropertyFlags,
    VkDeviceSize minOffsetAlignment)
    : lveDevice{device},
      instanceSize{instanceSize},
      instanceCount{instanceCount},
      usageFlags{usageFlags},
      memoryPropertyFlags{memoryPropertyFlags} {
  alignmentSize = getAlignment(instanceSize, minOffsetAlignment);
  bufferSize = alignmentSize * instanceCount;
  device.createBuffer(bufferSize, usageFlags, memoryPropertyFlags, buffer, memory);
}

LveBuffer::~LveBuffer() {
  unmap();
  vkDestroyBuffer(lveDevice.device(), buffer, nullptr);
  vkFreeMemory(lveDevice.device(), memory, nullptr);
}

VkResult LveBuffer::map(VkDeviceSize size, VkDeviceSize offset) {
  assert(buffer && memory && "cannot map buffer before creation");
  return vkMapMemory(lveDevice.device(), memory, offset, size, 0, &mapped);
}

void LveBuffer::unmap() {
  if (mapped) {
    vkUnmapMemory(lveDevice.device(), memory);
    mapped = nullptr;
  }
}

void LveBuffer::writeToBuffer(void *data, VkDeviceSize size, VkDeviceSize offset) {
  assert(mapped && "cannot write to unmapped buffer");
  if (size == VK_WHOLE_SIZE) {
    std::memcpy(mapped, data, bufferSize);
  } else {
    char *memPtr = (char *)mapped;
    memPtr += offset;
    std::memcpy(memPtr, data, size);
  }
}

VkResult LveBuffer::flush(VkDeviceSize size, VkDeviceSize offset) {
  VkMappedMemoryRange range{};
  range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  range.memory = memory;
  range.offset = offset;
  range.size = size;
  return vkFlushMappedMemoryRanges(lveDevice.device(), 1, &range);
}

VkResult LveBuffer::invalidate(VkDeviceSize size, VkDeviceSize offset) {
  VkMappedMemoryRange range{};
  range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  range.memory = memory;
  range.offset = offset;
  range.size = size;
  return vkInvalidateMappedMemoryRanges(lveDevice.device(), 1, &range);
}

VkDescriptorBufferInfo LveBuffer::descriptorInfo(VkDeviceSize size, VkDeviceSize offset) const noexcept {
  return VkDescriptorBufferInfo{buffer, offset, size};
}

void LveBuffer::writeToIndex(void *data, int index) {
  writeToBuffer(data, instanceSize, index * alignmentSize);
}

VkResult LveBuffer::flushIndex(int index) {
  return flush(alignmentSize, index * alignmentSize);
}

VkDescriptorBufferInfo LveBuffer::descriptorInfoForIndex(int index) const noexcept {
  return descriptorInfo(alignmentSize, index * alignmentSize);
}

VkResult LveBuffer::invalidateIndex(int index) {
  return invalidate(alignmentSize, index * alignmentSize);
}

}  // namespace lve
