#pragma once

#include "core/lve_device.hpp"
#include "renderer/lve_frame_info.hpp"
#include "renderer/lve_pipeline.hpp"
#include "scene/lve_game_object.hpp"

#include <im3d.h>

#include <memory>
#include <vector>

/**
 * im3d rendering system.
 * integrates the im3d immediate mode library with vulkan.
 * github: https://github.com/john-chapman/im3d
 */

namespace lve {

class Im3dSystem {
 public:
  Im3dSystem(LveDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
  ~Im3dSystem();

  Im3dSystem(const Im3dSystem &) = delete;
  Im3dSystem &operator=(const Im3dSystem &) = delete;

  void render(FrameInfo &frameInfo);
  static Im3d::Mat4 toIm3d(const glm::mat4& m) noexcept;

 private:
  void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
  void createPipelines(VkRenderPass renderPass);

  LveDevice &lveDevice;
  
  VkPipelineLayout pipelineLayout;
  std::unique_ptr<LvePipeline> pointsPipeline;
  std::unique_ptr<LvePipeline> linesPipeline;
  std::unique_ptr<LvePipeline> trianglesPipeline;

  std::unique_ptr<LveBuffer> dynamicVertexBuffer;
  
  struct Im3dVertex {
    glm::vec4 positionSize;
    uint32_t color;
  };
};

}  // namespace lve
