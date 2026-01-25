#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>

/**
 * window management wrapper for glfw.
 * handles vulkan surface creation and window event polling.
 * github: https://github.com/glfw/glfw
 */

namespace lve {

class LveWindow {
 public:
  LveWindow(int w, int h, std::string name);
  ~LveWindow();

  LveWindow(const LveWindow &) = delete;
  LveWindow &operator=(const LveWindow &) = delete;

  bool shouldClose() const noexcept { return glfwWindowShouldClose(window); }
  VkExtent2D getExtent() const noexcept { return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}; }
  bool wasWindowResized() const noexcept { return framebufferResized; }
  void resetWindowResizedFlag() noexcept { framebufferResized = false; }
  GLFWwindow *getGLFWwindow() const noexcept { return window; }

  /**
   * gets and resets the scroll offset since last frame.
   */
  double getScrollOffsetAndReset() noexcept {
    double offset = scrollYOffset;
    scrollYOffset = 0.0;
    return offset;
  }

  void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface);

 private:
  static void framebufferResizeCallback(GLFWwindow *window, int width, int height);
  static void scrollCallback(GLFWwindow *window, double xoffset, double yoffset);
  void initWindow();

  int width;
  int height;
  bool framebufferResized = false;
  double scrollYOffset = 0.0;

  std::string windowName;
  GLFWwindow *window;
};
}  // namespace lve
