#include "input/keyboard_movement_controller.hpp"

#include <limits>

/**
 * input controller implementation.
 * provides logic for free-look rotation and orientation-relative movement.
 */

namespace lve {

void KeyboardMovementController::moveFree(GLFWwindow* window, float dt, LveGameObject& obj) {
  bool currentRP = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
  glm::vec3 rot{0};

  if (currentRP) {
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    if (firstMouse || !rightMousePressed) {
      lastX = xpos; lastY = ypos;
      firstMouse = false;
    }
    float dx = static_cast<float>(xpos - lastX);
    float dy = static_cast<float>(lastY - ypos);
    lastX = xpos; lastY = ypos;

    rot.y += dx * mouseSensitivity / (dt + 0.0001f);
    rot.x += dy * mouseSensitivity / (dt + 0.0001f);
  }
  rightMousePressed = currentRP;

  if (glm::dot(rot, rot) > std::numeric_limits<float>::epsilon()) {
    obj.transform.rotation += lookSpeed * dt * rot;
  }

  obj.transform.rotation.x = glm::clamp(obj.transform.rotation.x, -1.5f, 1.5f);
  obj.transform.rotation.y = glm::mod(obj.transform.rotation.y, glm::two_pi<float>());

  float yaw = obj.transform.rotation.y;
  float pitch = obj.transform.rotation.x;
  const glm::vec3 fwd{sin(yaw) * cos(pitch), -sin(pitch), cos(yaw) * cos(pitch)};
  const glm::vec3 right{cos(yaw), 0.f, -sin(yaw)};
  const glm::vec3 up{0.f, -1.f, 0.f};

  glm::vec3 dir{0.f};
  if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) dir += fwd;
  if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) dir -= fwd;
  if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS) dir += right;
  if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) dir -= right;
  if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS) dir += up;
  if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS) dir -= up;

  if (glm::dot(dir, dir) > std::numeric_limits<float>::epsilon()) {
    float mult = 1.f;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) mult = 4.f;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) mult = 0.2f;
    obj.transform.translation += moveSpeed * dt * mult * glm::normalize(dir);
  }
}

void KeyboardMovementController::handleScroll(GLFWwindow* window, double yOffset, LveGameObject& obj) {
  float yaw = obj.transform.rotation.y;
  float pitch = obj.transform.rotation.x;
  glm::vec3 fwd{sin(yaw) * cos(pitch), -sin(pitch), cos(yaw) * cos(pitch)};
  
  float mult = 1.f;
  if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) mult = 4.f;
  if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) mult = 0.2f;
  obj.transform.translation += fwd * (static_cast<float>(yOffset) * scrollSpeed * mult);
}

}  // namespace lve