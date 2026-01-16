#include "first_app.hpp"

#include "keyboard_movement_controller.hpp"
#include "lve_buffer.hpp"
#include "lve_camera.hpp"
#include "systems/point_light_system.hpp"
#include "systems/simple_render_system.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <array>
#include <cassert>
#include <chrono>
#include <stdexcept>

namespace lve {

FirstApp::FirstApp() {
  globalPool =
      LveDescriptorPool::Builder(lveDevice)
          .setMaxSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT)
          .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
          .build();
  loadGameObjects();
}

FirstApp::~FirstApp() {}

void FirstApp::run() {
  std::vector<std::unique_ptr<LveBuffer>> uboBuffers(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
  for (int i = 0; i < uboBuffers.size(); i++) {
    uboBuffers[i] = std::make_unique<LveBuffer>(
        lveDevice,
        sizeof(GlobalUbo),
        1,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    uboBuffers[i]->map();
  }

  auto globalSetLayout =
      LveDescriptorSetLayout::Builder(lveDevice)
          .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
          .build();

  std::vector<VkDescriptorSet> globalDescriptorSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
  for (int i = 0; i < globalDescriptorSets.size(); i++) {
    auto bufferInfo = uboBuffers[i]->descriptorInfo();
    LveDescriptorWriter(*globalSetLayout, *globalPool)
        .writeBuffer(0, &bufferInfo)
        .build(globalDescriptorSets[i]);
  }

  vlmUi = std::make_unique<VlmUi>(
      lveDevice, lveRenderer.getSwapChainRenderPass(),
      lveRenderer.getSwapChainExtent().width, lveRenderer.getSwapChainExtent().height);

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
  while (!lveWindow.shouldClose()) {
    glfwPollEvents();
    
    // Toggle Menu
    static bool f1Pressed = false;
    if (glfwGetKey(lveWindow.getGLFWwindow(), GLFW_KEY_F1) == GLFW_PRESS) {
      if (!f1Pressed) {
        menuOpen = !menuOpen;
        glfwSetInputMode(lveWindow.getGLFWwindow(), GLFW_CURSOR, menuOpen ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
      }
      f1Pressed = true;
    } else {
      f1Pressed = false;
    }

    auto newTime = std::chrono::high_resolution_clock::now();
    float frameTime =
        std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
    currentTime = newTime;

    if (!menuOpen) {
      cameraController.moveInPlaneXZ(lveWindow.getGLFWwindow(), frameTime, viewerObject);
    } else {
      // Forward input to UI
      double x, y;
      glfwGetCursorPos(lveWindow.getGLFWwindow(), &x, &y);
      vlmUi->handleMouseMove(x, y);

      static bool leftMouseDown = false;
      bool currentLeftDown = glfwGetMouseButton(lveWindow.getGLFWwindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
      if (currentLeftDown && !leftMouseDown) {
        vlmUi->handleMouseButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
      } else if (!currentLeftDown && leftMouseDown) {
        vlmUi->handleMouseButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
      }
      leftMouseDown = currentLeftDown;
    }
    
    vlmUi->update();
    camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

    // Debug Info (FPS & Position)
    static float frameTimer = 0.0f;
    static int frameCount = 0;
    frameTimer += frameTime;
    frameCount++;
    if (frameTimer >= 0.1f) {
      float fps = frameCount / frameTimer;
      char title[256];
      snprintf(
          title,
          sizeof(title),
          "vlm engine | FPS: %.0f | Pos: {%.2f, %.2f, %.2f}",
          fps,
          viewerObject.transform.translation.x,
          viewerObject.transform.translation.y,
          viewerObject.transform.translation.z);
      glfwSetWindowTitle(lveWindow.getGLFWwindow(), title);
      
      // Update UI Telemetry
      vlmUi->updateTelemetry(fps, viewerObject.transform.translation.x, 
                            viewerObject.transform.translation.y, 
                            viewerObject.transform.translation.z);

      frameTimer = 0.0f;
      frameCount = 0;
    }

    float aspect = lveRenderer.getAspectRatio();
    VkExtent2D extent = lveRenderer.getSwapChainExtent();
    vlmUi->resize(extent.width, extent.height);

    camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);

    if (auto commandBuffer = lveRenderer.beginFrame()) {
      int frameIndex = lveRenderer.getFrameIndex();
      FrameInfo frameInfo{
          frameIndex,
          frameTime,
          commandBuffer,
          camera,
          globalDescriptorSets[frameIndex],
          gameObjects};

      // update
      GlobalUbo ubo{};
      ubo.projection = camera.getProjection();
      ubo.view = camera.getView();
      ubo.inverseView = camera.getInverseView();
      pointLightSystem.update(frameInfo, ubo);
      uboBuffers[frameIndex]->writeToBuffer(&ubo);
      uboBuffers[frameIndex]->flush();

      // render
      lveRenderer.beginSwapChainRenderPass(commandBuffer);

      // order here matters
      simpleRenderSystem.renderGameObjects(frameInfo);
      pointLightSystem.render(frameInfo);
      vlmUi->render(commandBuffer);

      lveRenderer.endSwapChainRenderPass(commandBuffer);
      lveRenderer.endFrame();
    }
  }

  vkDeviceWaitIdle(lveDevice.device());
}

void FirstApp::loadGameObjects() {
  std::shared_ptr<LveModel> lveModel =
      LveModel::createModelFromFile(lveDevice, "models/Ships_propeller.obj");
  auto propeller = LveGameObject::createGameObject();
  propeller.model = lveModel;
  propeller.transform.translation = {0.f, .5f, 0.f};
  propeller.transform.scale = {.005f, .005f, .005f};
  gameObjects.emplace(propeller.getId(), std::move(propeller));

  lveModel = LveModel::createModelFromFile(lveDevice, "models/flat_vase.obj");
  auto flatVase = LveGameObject::createGameObject();
  flatVase.model = lveModel;
  flatVase.transform.translation = {-.5f, .5f, 0.f};
  flatVase.transform.scale = {3.f, 1.5f, 3.f};
  gameObjects.emplace(flatVase.getId(), std::move(flatVase));

  lveModel = LveModel::createModelFromFile(lveDevice, "models/quad.obj");
  auto floor = LveGameObject::createGameObject();
  floor.model = lveModel;
  floor.transform.translation = {0.f, .5f, 0.f};
  floor.transform.scale = {3.f, 1.f, 3.f};
  gameObjects.emplace(floor.getId(), std::move(floor));

  std::vector<glm::vec3> lightColors{
      {1.f, .1f, .1f},
      {.1f, .1f, 1.f},
      {.1f, 1.f, .1f},
      {1.f, 1.f, .1f},
      {.1f, 1.f, 1.f},
      {1.f, 1.f, 1.f}  //
  };

  for (int i = 0; i < lightColors.size(); i++) {
    auto pointLight = LveGameObject::makePointLight(0.2f);
    pointLight.color = lightColors[i];
    auto rotateLight = glm::rotate(
        glm::mat4(1.f),
        (i * glm::two_pi<float>()) / lightColors.size(),
        {0.f, -1.f, 0.f});
    pointLight.transform.translation = glm::vec3(rotateLight * glm::vec4(-1.f, -1.f, -1.f, 1.f));
    gameObjects.emplace(pointLight.getId(), std::move(pointLight));
  }
}

}  // namespace lve
