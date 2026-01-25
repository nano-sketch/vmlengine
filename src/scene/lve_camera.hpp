#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

/**
 * camera system for viewing the 3d scene.
 * manages projection and view matrices with various orientation methods.
 */

namespace lve {

class LveCamera {
 public:
  void setOrthographicProjection(float left, float right, float top, float bottom, float near, float far);
  void setPerspectiveProjection(float fovy, float aspect, float near, float far);

  void setViewDirection(glm::vec3 pos, glm::vec3 dir, glm::vec3 up = {0.f, -1.f, 0.f});
  void setViewTarget(glm::vec3 pos, glm::vec3 target, glm::vec3 up = {0.f, -1.f, 0.f});
  void setViewYXZ(glm::vec3 pos, glm::vec3 rot);

  const glm::mat4& getProjection() const noexcept { return projectionMatrix; }
  const glm::mat4& getView() const noexcept { return viewMatrix; }
  const glm::mat4& getInverseView() const noexcept { return inverseViewMatrix; }
  glm::vec3 getPosition() const noexcept { return glm::vec3(inverseViewMatrix[3]); }

 private:
  glm::mat4 projectionMatrix{1.f};
  glm::mat4 viewMatrix{1.f};
  glm::mat4 inverseViewMatrix{1.f};
};

}  // namespace lve