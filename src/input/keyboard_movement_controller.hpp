#pragma once

#include "scene/lve_game_object.hpp"
#include "core/lve_window.hpp"

/**
 * input controller for camera navigation.
 * implements free fly movement with roblox-style mouse rotation.
 */

namespace lve {

class KeyboardMovementController {
 public:
  struct KeyMappings {
    int moveLeft = GLFW_KEY_A;
    int moveRight = GLFW_KEY_D;
    int moveForward = GLFW_KEY_W;
    int moveBackward = GLFW_KEY_S;
    int moveUp = GLFW_KEY_E;
    int moveDown = GLFW_KEY_Q;
    int lookLeft = GLFW_KEY_LEFT;
    int lookRight = GLFW_KEY_RIGHT;
    int lookUp = GLFW_KEY_UP;
    int lookDown = GLFW_KEY_DOWN;
  };

  void moveFree(GLFWwindow* window, float dt, LveGameObject& gameObject);
  void handleScroll(GLFWwindow* window, double yOffset, LveGameObject& gameObject);
  void resetInput() noexcept { firstMouse = true; rightMousePressed = false; }

  KeyMappings keys{};
  float moveSpeed{3.f};
  float lookSpeed{1.5f};
  float mouseSensitivity{0.002f};
  float scrollSpeed{0.5f};

 private:
  bool firstMouse = true;
  bool rightMousePressed = false;
  double lastX = 0, lastY = 0;
};

}  // namespace lve