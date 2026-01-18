#include "first_app.hpp"
#include "core/lve_window.hpp"
#include "input/keyboard_movement_controller.hpp"
#include "renderer/lve_buffer.hpp"
#include "scene/lve_camera.hpp"
#include "systems/point_light_system.hpp"
#include "systems/simple_render_system.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <array>
#include <cassert>
#include <chrono>
#include <stdexcept>
#include <numeric>

namespace lve {



FirstApp::FirstApp() {
  globalPool = LveDescriptorPool::Builder(lveDevice)
                   .setMaxSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT)
                   .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
                   .build();
  loadGameObjects();
}

FirstApp::~FirstApp() {}

void FirstApp::run() {
  std::vector<std::unique_ptr<LveBuffer>> uboBuffers(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
  for (auto& buffer : uboBuffers) {
    buffer = std::make_unique<LveBuffer>(
        lveDevice,
        sizeof(GlobalUbo),
        1,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    buffer->map();
  }

  auto globalSetLayout =
      LveDescriptorSetLayout::Builder(lveDevice)
          .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
          .build();

  std::vector<VkDescriptorSet> globalDescriptorSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
  for (size_t i = 0; i < globalDescriptorSets.size(); i++) {
    auto bufferInfo = uboBuffers[i]->descriptorInfo();
    LveDescriptorWriter(*globalSetLayout, *globalPool)
        .writeBuffer(0, &bufferInfo)
        .build(globalDescriptorSets[i]);
  }

  const auto& swapChainExtent = lveRenderer.getSwapChainExtent();
  vlmUi = std::make_unique<VlmUi>(
      lveDevice, 
      lveRenderer.getSwapChainRenderPass(),
      swapChainExtent.width, 
      swapChainExtent.height);

  SimpleRenderSystem simpleRenderSystem{
      lveDevice,
      lveRenderer.getSwapChainRenderPass(),
      globalSetLayout->getDescriptorSetLayout()};
      
  PointLightSystem pointLightSystem{
      lveDevice,
      lveRenderer.getSwapChainRenderPass(),
      globalSetLayout->getDescriptorSetLayout()};
      
  LveCamera camera{};
  auto viewerObject = LveGameObject::createGameObject();
  viewerObject.transform.translation.z = -2.5f;
  KeyboardMovementController cameraController{};

  auto currentTime = std::chrono::high_resolution_clock::now();
  
  // HUD/Menu State
  bool f1Pressed = false;
  float frameTimer = 0.0f;
  int frameCount = 0;

  while (!lveWindow.shouldClose()) {
    glfwPollEvents();

    auto newTime = std::chrono::high_resolution_clock::now();
    float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
    currentTime = newTime;
    
    // clamp frameTime to avoid spiral of death on lag spikes
    frameTime = std::min(frameTime, 0.1f);

    // Toggle Menu Input
    if (glfwGetKey(lveWindow.getGLFWwindow(), GLFW_KEY_F1) == GLFW_PRESS) {
      if (!f1Pressed) {
        menuOpen = !menuOpen;
        glfwSetInputMode(lveWindow.getGLFWwindow(), GLFW_CURSOR, menuOpen ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
        if (!menuOpen) cameraController.resetInput();
      }
      f1Pressed = true;
    } else {
      f1Pressed = false;
    }

    if (!menuOpen) {
      cameraController.moveFree(lveWindow.getGLFWwindow(), frameTime, viewerObject);
    } else {
      double x, y;
      glfwGetCursorPos(lveWindow.getGLFWwindow(), &x, &y);
      vlmUi->handleMouseMove(x, y);

      static bool leftMouseDown = false;
      bool currentLeftDown = glfwGetMouseButton(lveWindow.getGLFWwindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
      
      if (currentLeftDown != leftMouseDown) {
        vlmUi->handleMouseButton(GLFW_MOUSE_BUTTON_LEFT, currentLeftDown ? GLFW_PRESS : GLFW_RELEASE, 0);
        leftMouseDown = currentLeftDown;
      }
    }

    vlmUi->update();
    camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

    // Performance Counters
    frameTimer += frameTime;
    frameCount++;
    if (frameTimer >= 0.2f) { // Update title every 200ms for readability
      glfwSetWindowTitle(lveWindow.getGLFWwindow(), "vlm engine");
      
      // Pass telemetry to UI if needed, otherwise this is just internal tracking
      vlmUi->updateTelemetry(frameCount / frameTimer, 
                            viewerObject.transform.translation.x, 
                            viewerObject.transform.translation.y, 
                            viewerObject.transform.translation.z);
      
      frameTimer = 0.0f;
      frameCount = 0;
    }

    float aspect = lveRenderer.getAspectRatio();
    camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);
    
    // Sync UI with Swapchain
    const auto& extent = lveRenderer.getSwapChainExtent();
    if (vlmUi) vlmUi->resize(extent.width, extent.height);

    if (auto commandBuffer = lveRenderer.beginFrame()) {
      int frameIndex = lveRenderer.getFrameIndex();
      FrameInfo frameInfo{
          frameIndex,
          frameTime,
          commandBuffer,
          camera,
          globalDescriptorSets[frameIndex],
          gameObjects};

      // update global UBO
      GlobalUbo ubo{};
      ubo.projection = camera.getProjection();
      ubo.view = camera.getView();
      ubo.inverseView = camera.getInverseView();
      
      pointLightSystem.update(frameInfo, ubo);
      
      uboBuffers[frameIndex]->writeToBuffer(&ubo);
      uboBuffers[frameIndex]->flush();

      // render
      lveRenderer.beginSwapChainRenderPass(commandBuffer);
      
      simpleRenderSystem.renderGameObjects(frameInfo);
      pointLightSystem.render(frameInfo);
      vlmUi->render(commandBuffer); // UI overlay last

      lveRenderer.endSwapChainRenderPass(commandBuffer);
      lveRenderer.endFrame();
    }
  }

  vkDeviceWaitIdle(lveDevice.device());
}

void FirstApp::loadGameObjects() {
  // Helper lambda for loading models securely
  auto loadModel = [&](const std::string& path, const glm::vec3& translation, const glm::vec3& scale) {
    auto model = LveModel::createModelFromFile(lveDevice, path);
    auto obj = LveGameObject::createGameObject();
    obj.model = std::move(model);
    obj.transform.translation = translation;
    obj.transform.scale = scale;
    gameObjects.emplace(obj.getId(), std::move(obj));
  };

  loadModel("models/Ships_propeller.obj", {0.f, .5f, 0.f}, {.005f, .005f, .005f});
  loadModel("models/flat_vase.obj", {-.5f, .5f, 0.f}, {3.f, 1.5f, 3.f});
  loadModel("models/quad.obj", {0.f, .5f, 0.f}, {3.f, 1.f, 3.f});

  const std::vector<glm::vec3> lightColors{
      {1.f, .1f, .1f}, {.1f, .1f, 1.f}, {.1f, 1.f, .1f},
      {1.f, 1.f, .1f}, {.1f, 1.f, 1.f}, {1.f, 1.f, 1.f}
  };

  for (size_t i = 0; i < lightColors.size(); i++) {
    auto pointLight = LveGameObject::makePointLight(0.2f);
    pointLight.color = lightColors[i];
    
    // Rotate lights in a circle
    auto rotation = glm::rotate(
        glm::mat4(1.f),
        (i * glm::two_pi<float>()) / lightColors.size(),
        {0.f, -1.f, 0.f});
        
    pointLight.transform.translation = glm::vec3(rotation * glm::vec4(-1.f, -1.f, -1.f, 1.f));
    gameObjects.emplace(pointLight.getId(), std::move(pointLight));
  }
}

}  // namespace lve
