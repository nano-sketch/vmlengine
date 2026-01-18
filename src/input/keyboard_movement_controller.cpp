#include "input/keyboard_movement_controller.hpp"

// std
#include <limits>

namespace lve {

void KeyboardMovementController::moveFree(
    GLFWwindow* window, float dt, LveGameObject& gameObject) {
  glm::vec3 rotate{0};
  // mouse rotation
  double xpos, ypos;
  glfwGetCursorPos(window, &xpos, &ypos);

  if (firstMouse) {
    lastX = xpos;
    lastY = ypos;
    firstMouse = false;
  }

  float xoffset = static_cast<float>(xpos - lastX);
  float yoffset = static_cast<float>(lastY - ypos); // reversed since y-coordinates go from bottom to top
  lastX = xpos;
  lastY = ypos;

  rotate.y += xoffset * mouseSensitivity / (dt + 0.0001f);
  rotate.x += yoffset * mouseSensitivity / (dt + 0.0001f);

  if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon()) {
    gameObject.transform.rotation += lookSpeed * dt * rotate;
  }

  // limit pitch values between about +/- 85ish degrees
  gameObject.transform.rotation.x = glm::clamp(gameObject.transform.rotation.x, -1.5f, 1.5f);
  gameObject.transform.rotation.y = glm::mod(gameObject.transform.rotation.y, glm::two_pi<float>());

  float yaw = gameObject.transform.rotation.y;
  float pitch = gameObject.transform.rotation.x;
  
  // calculate 3D forward direction
  // standard vulkan convention: Forward is -Z.
  // we use standard trig to get a vector, then negate it or adjust components to point into -Z.
  // converting spherical (yaw/pitch) to Cartesian where Yaw=0 -> -Z
  const glm::vec3 forwardDir{
      sin(yaw) * cos(pitch), 
      -sin(pitch), 
      cos(yaw) * cos(pitch)
  };
  
  // calculate Right direction (always horizontal)
  const glm::vec3 rightDir{forwardDir.z, 0.f, -forwardDir.x};
  
  // world Up
  const glm::vec3 upDir{0.f, -1.f, 0.f};

  glm::vec3 moveDir{0.f};
  // normalize vectors before adding to ensure consistent speed
  const glm::vec3 horizontalRight{ cos(yaw), 0.f, -sin(yaw) };
  
  if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir += forwardDir;
  if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir -= forwardDir;
  if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS) moveDir += horizontalRight;
  if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) moveDir -= horizontalRight;
  if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS) moveDir += upDir;
  if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS) moveDir -= upDir;

  if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
    gameObject.transform.translation += moveSpeed * dt * glm::normalize(moveDir);
  }
}
}  // namespace lve