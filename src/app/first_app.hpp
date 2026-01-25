#pragma once

#include "core/lve_device.hpp"
#include "renderer/lve_descriptors.hpp"
#include "scene/lve_game_object.hpp"
#include "renderer/lve_renderer.hpp"
#include "core/lve_window.hpp"
#include "ui/vlm_ui.hpp"
#include "renderer/lve_shadow_map.hpp"
#include "systems/shadow_system.hpp"

// std
#include <memory>
#include <vector>

namespace lve {

/**
 * main engine class that manages high level orchestration.
 * 
 * firstapp handles vulkan initialization, the main render loop,
 * global descriptor pools, and coordinates various render systems.
 * it is the entry point for the engines runtime behavior.
 */
class FirstApp {
 public:
  static constexpr int WIDTH = 1200;
  static constexpr int HEIGHT = 800;

  /**
   * initializes the app, creating the device, window, and initial scene.
   */
  FirstApp();

  /**
   * standard destructor for resource cleanup.
   */
  ~FirstApp();

  // non copyable application
  FirstApp(const FirstApp &) = delete;
  FirstApp &operator=(const FirstApp &) = delete;

  /**
   * runs the main application loop.
   * 
   * this method runs until the window is closed, handling input events,
   * updating scene state, and submitting frames to the gpu.
   */
  void run();

 private:
  /**
   * factory method for populating the scene with default game objects and lights.
   */
  void loadGameObjects();
  void loadTransforms();
  void saveTransforms();

  // core windowing and device handles
  LveWindow lveWindow{WIDTH, HEIGHT, "vlm engine"};
  LveDevice lveDevice{lveWindow};
  LveRenderer lveRenderer{lveWindow, lveDevice};

  // global resource management
  std::unique_ptr<LveDescriptorPool> globalPool{};
  LveGameObject::Map gameObjects;
  
  // high level subsystems
  std::unique_ptr<VlmUi> vlmUi;
  std::unique_ptr<LveShadowMap> shadowMap;
  std::unique_ptr<ShadowSystem> shadowSystem;
  
  // descriptor sets
  VkDescriptorSet shadowDescriptorSet;

  /** tracks whether the internal f1 dev menu is currently displayed. */
  bool menuOpen = false;
  
  /** tracks whether f3 edit mode is active for object selection and manipulation. */
  bool editMode = false;
  
  /** id of the currently selected game object. */
  LveGameObject::id_t selectedObjectId = std::numeric_limits<LveGameObject::id_t>::max();
};
}  // namespace lve
