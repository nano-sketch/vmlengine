#pragma once

#include "scene/lve_game_object.hpp"
#include "core/lve_window.hpp"

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
  void resetInput() { firstMouse = true; }

  KeyMappings keys{};
  float moveSpeed{3.f};
  float lookSpeed{1.5f};
  float mouseSensitivity{0.002f};

 private:
  bool firstMouse = true;
  double lastX, lastY;
};
}  // namespace lve