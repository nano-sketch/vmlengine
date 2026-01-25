#include "renderer/lve_descriptors.hpp"

#include <cassert>
#include <stdexcept>

/**
 * descriptor implementation.
 * provides mechanisms for binding and updating shader resources.
 */

namespace lve {

// set layout builder

LveDescriptorSetLayout::Builder &LveDescriptorSetLayout::Builder::addBinding(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t count) {
  assert(bindings.count(binding) == 0 && "binding already in use");
  VkDescriptorSetLayoutBinding binding_struct{};
  binding_struct.binding = binding;
  binding_struct.descriptorType = descriptorType;
  binding_struct.descriptorCount = count;
  binding_struct.stageFlags = stageFlags;
  binding_struct.pImmutableSamplers = nullptr;
  bindings[binding] = binding_struct;
  return *this;
}

std::unique_ptr<LveDescriptorSetLayout> LveDescriptorSetLayout::Builder::build() const {
  return std::make_unique<LveDescriptorSetLayout>(lveDevice, bindings);
}

// set layout

LveDescriptorSetLayout::LveDescriptorSetLayout(LveDevice &lveDevice, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings)
    : lveDevice{lveDevice}, bindings{bindings} {
  std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
  setLayoutBindings.reserve(bindings.size());
  for (const auto& kv : bindings) setLayoutBindings.push_back(kv.second);

  VkDescriptorSetLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
  info.pBindings = setLayoutBindings.data();
  if (vkCreateDescriptorSetLayout(lveDevice.device(), &info, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout");
  }
}

LveDescriptorSetLayout::~LveDescriptorSetLayout() {
  vkDestroyDescriptorSetLayout(lveDevice.device(), descriptorSetLayout, nullptr);
}

// pool builder

LveDescriptorPool::Builder &LveDescriptorPool::Builder::addPoolSize(VkDescriptorType descriptorType, uint32_t count) {
  poolSizes.push_back({descriptorType, count});
  return *this;
}

LveDescriptorPool::Builder &LveDescriptorPool::Builder::setPoolFlags(VkDescriptorPoolCreateFlags flags) {
  poolFlags = flags;
  return *this;
}

LveDescriptorPool::Builder &LveDescriptorPool::Builder::setMaxSets(uint32_t count) {
  maxSets = count;
  return *this;
}

std::unique_ptr<LveDescriptorPool> LveDescriptorPool::Builder::build() const {
  return std::make_unique<LveDescriptorPool>(lveDevice, maxSets, poolFlags, poolSizes);
}

// pool

LveDescriptorPool::LveDescriptorPool(LveDevice &lveDevice, uint32_t maxSets, VkDescriptorPoolCreateFlags poolFlags, const std::vector<VkDescriptorPoolSize> &poolSizes)
    : lveDevice{lveDevice} {
  VkDescriptorPoolCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  info.flags = poolFlags;
  info.maxSets = maxSets;
  info.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  info.pPoolSizes = poolSizes.data();
  if (vkCreateDescriptorPool(lveDevice.device(), &info, nullptr, &descriptorPool) != VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor pool");
  }
}

LveDescriptorPool::~LveDescriptorPool() {
  vkDestroyDescriptorPool(lveDevice.device(), descriptorPool, nullptr);
}

bool LveDescriptorPool::allocateDescriptor(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet &descriptor) const {
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = descriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &descriptorSetLayout;
  return vkAllocateDescriptorSets(lveDevice.device(), &allocInfo, &descriptor) == VK_SUCCESS;
}

void LveDescriptorPool::freeDescriptors(std::vector<VkDescriptorSet> &descriptors) const {
  vkFreeDescriptorSets(lveDevice.device(), descriptorPool, static_cast<uint32_t>(descriptors.size()), descriptors.data());
}

void LveDescriptorPool::resetPool() {
  vkResetDescriptorPool(lveDevice.device(), descriptorPool, 0);
}

// writer

LveDescriptorWriter::LveDescriptorWriter(LveDescriptorSetLayout &setLayout, LveDescriptorPool &pool)
    : setLayout{setLayout}, pool{pool} {}

LveDescriptorWriter &LveDescriptorWriter::writeBuffer(uint32_t binding, VkDescriptorBufferInfo *bufferInfo) {
  assert(setLayout.bindings.count(binding) == 1 && "layout does not contain binding");
  auto &desc = setLayout.bindings[binding];
  assert(desc.descriptorCount == 1 && "binding expects multiple descriptors");
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstBinding = binding;
  write.descriptorCount = 1;
  write.descriptorType = desc.descriptorType;
  write.pBufferInfo = bufferInfo;
  writes.push_back(write);
  return *this;
}

LveDescriptorWriter &LveDescriptorWriter::writeImage(uint32_t binding, VkDescriptorImageInfo *imageInfo) {
  assert(setLayout.bindings.count(binding) == 1 && "layout does not contain binding");
  auto &desc = setLayout.bindings[binding];
  assert(desc.descriptorCount == 1 && "binding expects multiple descriptors");
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstBinding = binding;
  write.descriptorCount = 1;
  write.descriptorType = desc.descriptorType;
  write.pImageInfo = imageInfo;
  writes.push_back(write);
  return *this;
}

bool LveDescriptorWriter::build(VkDescriptorSet &set) {
  if (!pool.allocateDescriptor(setLayout.getDescriptorSetLayout(), set)) return false;
  overwrite(set);
  return true;
}

void LveDescriptorWriter::overwrite(VkDescriptorSet &set) {
  for (auto &write : writes) write.dstSet = set;
  vkUpdateDescriptorSets(pool.lveDevice.device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

}  // namespace lve
