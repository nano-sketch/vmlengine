#pragma once

#include "renderer/lve_descriptors.hpp"
#include "core/lve_device.hpp"
#include "scene/lve_game_object.hpp"
#include "renderer/lve_renderer.hpp"
#include "core/lve_window.hpp"
#include "ui/vlm_ui.hpp"

// std
#include <memory>
#include <vector>

namespace lve {
class FirstApp {
 public:
  static constexpr int WIDTH = 800;
  static constexpr int HEIGHT = 600;

  FirstApp();
  ~FirstApp();

  FirstApp(const FirstApp &) = delete;
  FirstApp &operator=(const FirstApp &) = delete;

  void run();

 private:
  void loadGameObjects();

  LveWindow lveWindow{WIDTH, HEIGHT, "vlm engine"};
  LveDevice lveDevice{lveWindow};
  LveRenderer lveRenderer{lveWindow, lveDevice};

  // note: order of declarations matters
  std::unique_ptr<LveDescriptorPool> globalPool{};
  LveGameObject::Map gameObjects;
  
  std::unique_ptr<VlmUi> vlmUi;
  bool menuOpen = false;
};
}  // namespace lve
