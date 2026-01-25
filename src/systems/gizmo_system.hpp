/*
 * Gizmo rendering system for 3D object manipulation.
 * 
 * Renders visual axis arrows (X/Y/Z) at the position of selected objects,
 * providing visual feedback for object transformation operations.
 */

#pragma once

#include "core/lve_device.hpp"
#include "renderer/lve_frame_info.hpp"
#include "renderer/lve_pipeline.hpp"
#include "scene/lve_game_object.hpp"

// std
#include <memory>
#include <vector>

namespace lve {

/**
 * @brief System for rendering 3D manipulation gizmos.
 * 
 * Gizmos are visual indicators (colored axis arrows) that appear at the center
 * of selected objects. They provide visual feedback for object orientation and
 * can be used for interactive transformation (translation, rotation, scaling).
 * 
 * The gizmo consists of three arrows:
 * - Red arrow: X-axis (right)
 * - Green arrow: Y-axis (up)
 * - Blue arrow: Z-axis (forward)
 */
class GizmoSystem {
 public:
  /**
   * @brief Constructs the gizmo rendering system.
   * 
   * @param device Vulkan device for resource creation.
   * @param renderPass Target render pass for gizmo rendering.
   * @param globalSetLayout Descriptor set layout for global UBO (camera matrices).
   */
  GizmoSystem(LveDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
  
  /**
   * @brief Destructor - cleans up Vulkan resources.
   */
  ~GizmoSystem();

  // Non-copyable
  GizmoSystem(const GizmoSystem &) = delete;
  GizmoSystem &operator=(const GizmoSystem &) = delete;

  /**
   * @brief Renders the gizmo at the specified world position.
   * 
   * The gizmo is rendered with depth testing disabled to ensure it's always
   * visible, even when behind other geometry. Each axis is rendered with its
   * corresponding color (X=red, Y=green, Z=blue).
   * 
   * @param frameInfo Current frame rendering context.
   * @param position World-space position to render the gizmo.
   * @param scale Size multiplier for the gizmo (default: 1.0).
   */
  void renderGizmo(FrameInfo &frameInfo, glm::vec3 position, float scale = 1.0f);

 private:
  /**
   * @brief Creates the graphics pipeline for gizmo rendering.
   * 
   * Configures the pipeline with:
   * - Depth testing disabled (always visible)
   * - Alpha blending enabled for smooth appearance
   * - Line topology for arrow shafts
   * 
   * @param renderPass Target render pass.
   */
   
   // add constants for scale?
  void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
  void createPipeline(VkRenderPass renderPass);
  
  /**
   * @brief Generates geometry for a single axis arrow.
   * 
   * Creates vertices for an arrow consisting of:
   * - A line shaft along the axis
   * - A cone arrowhead at the tip
   * 
   * @param axis Direction vector (unit length).
   * @param color RGB color for this axis.
   * @param length Length of the arrow shaft.
   * @return Vector of vertex positions and colors.
   */
  void createGizmoGeometry();

  LveDevice &lveDevice;
  
  std::unique_ptr<LvePipeline> lvePipeline;
  VkPipelineLayout pipelineLayout;
  
  std::unique_ptr<LveBuffer> vertexBuffer;  ///< Gizmo geometry vertices
  uint32_t vertexCount;                      ///< Number of vertices to draw
};

}  // namespace lve
