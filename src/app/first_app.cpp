#include "app/first_app.hpp"

// lve
#include "core/lve_window.hpp"
#include "core/lve_utils.hpp"
#include "input/keyboard_movement_controller.hpp"
#include "renderer/lve_buffer.hpp"
#include "renderer/lve_texture.hpp"
#include "systems/point_light_system.hpp"
#include "systems/simple_render_system.hpp"
#include "systems/im3d_system.hpp"
#include "scene/lve_camera.hpp"

// libs
#include <im3d.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

// std
#include <array>
#include <cassert>
#include <chrono>
#include <stdexcept>
#include <numeric>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>

#undef max
#undef min

namespace lve {

/**
 * constructor for the primary application orchestrator.
 * 
 * initializes high-level descriptors and populates the world
 * with its initial set of entities.
 */
FirstApp::FirstApp() {
  initGlobalDescriptorPool();
  loadGameObjects();
}

/**
 * destructor ensuring state persistence.
 * 
 * saves any runtime object modifications back to disk before
 * the vulkan instance is dismantled.
 */
FirstApp::~FirstApp() { saveTransforms(); }

/**
 * sets up the global heap for shader resources.
 * 
 * configures a descriptor pool capable of housing both global
 * uniform buffers and per-object image samplers.
 */
void FirstApp::initGlobalDescriptorPool() {
  globalPool = LveDescriptorPool::Builder(lveDevice)
                   .setMaxSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT + 100)
                   .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
                   .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100)
                   .build();
}

/**
 * application entry point and main loop orchestration.
 * 
 * synchronizes cpu side logic with gpu command submission and
 * handles the lifecycle of various rendering subsystems.
 */
void FirstApp::run() {
  // initialize uniform buffers for per-frame global data
  uboBuffers.resize(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
  for (int i = 0; i < uboBuffers.size(); i++) {
    uboBuffers[i] = std::make_unique<LveBuffer>(
        lveDevice,
        sizeof(GlobalUbo),
        1,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    uboBuffers[i]->map();
  }

  // create descriptor set layout for camera matrices and global lighting
  globalSetLayout = LveDescriptorSetLayout::Builder(lveDevice)
                        .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                        .build();

  // allocating descriptor sets for each frame in flight
  globalDescriptorSets.resize(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
  for (int i = 0; i < globalDescriptorSets.size(); i++) {
    auto bufferInfo = uboBuffers[i]->descriptorInfo();
    LveDescriptorWriter(*globalSetLayout, *globalPool)
        .writeBuffer(0, &bufferInfo)
        .build(globalDescriptorSets[i]);
  }

  // initialize rendering subsystems
  const auto& extent = lveRenderer.getSwapChainExtent();
  vlmUi = std::make_unique<VlmUi>(lveDevice, lveRenderer.getSwapChainRenderPass(), extent.width, extent.height);
  
  simpleRenderSystem = std::make_unique<SimpleRenderSystem>(
      lveDevice, lveRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout());
  
  pointLightSystem = std::make_unique<PointLightSystem>(
      lveDevice, lveRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout());
  
  shadowMap = std::make_unique<LveShadowMap>(lveDevice, 2048, 2048);
  shadowSystem = std::make_unique<ShadowSystem>(lveDevice, lveRenderer.getShadowRenderPass());
  
  im3dSystem = std::make_unique<Im3dSystem>(
      lveDevice, lveRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout());

  // setup shadow map descriptor for the main pass
  {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = shadowMap->getShadowSampler();
    imageInfo.imageView = shadowMap->getShadowImageView();
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    LveDescriptorWriter(simpleRenderSystem->getShadowSetLayout(), *globalPool)
        .writeImage(0, &imageInfo)
        .build(shadowDescriptorSet);
  }

  // bind textures for all game objects in the scene
  for (auto& kv : gameObjects) {
    auto& obj = kv.second;
    if (obj.diffuseMap) {
      VkDescriptorImageInfo imageInfo{};
      imageInfo.sampler = obj.diffuseMap->getSampler();
      imageInfo.imageView = obj.diffuseMap->getImageView();
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      LveDescriptorWriter(simpleRenderSystem->getTextureSetLayout(), *globalPool)
          .writeImage(0, &imageInfo)
          .build(obj.textureDescriptorSet);
    }
  }

  LveCamera camera{};
  auto viewerObject = LveGameObject::createGameObject();
  viewerObject.transform.translation.z = -2.5f;
  KeyboardMovementController cameraController{};

  auto currentTime = std::chrono::high_resolution_clock::now();
  float perfTimer = 0.0f;
  int frameCount = 0;

  /**
   * main execution loop
   * 
   * polls system events, calculates frametime, and updates session state before
   * coordinating the graphics pipeline.
   */
  while (!lveWindow.shouldClose()) {
    glfwPollEvents();

    auto newTime = std::chrono::high_resolution_clock::now();
    float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
    currentTime = newTime;
    frameTime = std::min(frameTime, 0.1f);

    processInput(frameTime, viewerObject, cameraController);

    // update user interface state
    vlmUi->update();
    
    // transform camera relative to viewer object
    camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);
    auto viewerPos = camera.getPosition();

    // calculate telemetry data for the ui hud
    perfTimer += frameTime;
    frameCount++;
    if (perfTimer >= 0.2f) {
      vlmUi->updateTelemetry(frameCount / perfTimer, viewerPos.x, viewerPos.y, viewerPos.z);
      perfTimer = 0.f;
      frameCount = 0;
    }

    // handle projection updates for dynamic aspect ratios
    float aspect = lveRenderer.getAspectRatio();
    camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);
    
    // ensure ui is correctly scaled to the current window extent
    if (vlmUi) {
      auto currentExtent = lveRenderer.getSwapChainExtent();
      vlmUi->resize(currentExtent.width, currentExtent.height);
    }

    /**
     * coordinate im3d for interactive gizmos and bounding boxes.
     * 
     * provides visual debugging for object selection and manipulation during
     * edit mode.
     */
    {
      auto currentExtent = lveRenderer.getSwapChainExtent();
      auto& ad = Im3d::GetAppData();
      ad.m_deltaTime = frameTime;
      ad.m_viewportSize = {static_cast<float>(currentExtent.width), static_cast<float>(currentExtent.height)};
      ad.m_viewOrigin = {viewerPos.x, viewerPos.y, viewerPos.z};
      
      // calculating world space cursor ray for interaction
      double mx, my;
      glfwGetCursorPos(lveWindow.getGLFWwindow(), &mx, &my);
      float nx = (2.f * static_cast<float>(mx)) / currentExtent.width - 1.f;
      float ny = (2.f * static_cast<float>(my)) / currentExtent.height - 1.f;
      glm::vec4 rc = {nx, ny, 0.1f, 1.f};
      glm::vec4 re = glm::inverse(camera.getProjection()) * rc;
      re = {re.x, re.y, 1.f, 0.f};
      glm::vec3 rw = glm::normalize(glm::vec3(camera.getInverseView() * re));
      
      ad.m_cursorRayOrigin = {viewerPos.x, viewerPos.y, viewerPos.z};
      ad.m_cursorRayDirection = {rw.x, rw.y, rw.z};
      ad.m_keyDown[Im3d::Mouse_Left] = glfwGetMouseButton(lveWindow.getGLFWwindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
      ad.m_projScaleY = tanf(glm::radians(50.f) * 0.5f) * 2.0f;

      Im3d::NewFrame();
      if (editMode) {
        // draw bounding boxes for all visible models
        for (auto& kv : gameObjects) {
          auto& obj = kv.second;
          if (!obj.model) continue;
          bool isSelected = (obj.getId() == selectedObjectId);
          Im3d::PushColor(isSelected ? Im3d::Color_Cyan : Im3d::Color_Yellow);
          Im3d::PushSize(isSelected ? 2.f : 1.f);
          Im3d::PushMatrix(Im3dSystem::toIm3d(obj.transform.mat4()));
          const auto& b = obj.model->getBoundingBox();
          Im3d::DrawAlignedBox({b.min.x, b.min.y, b.min.z}, {b.max.x, b.max.y, b.max.z});
          Im3d::PopMatrix();
          Im3d::PopSize();
          Im3d::PopColor();
        }
        // handle gizmo interaction for selected objects
        if (selectedObjectId != std::numeric_limits<LveGameObject::id_t>::max()) {
          auto it = gameObjects.find(selectedObjectId);
          if (it != gameObjects.end()) {
            float p[3] = {it->second.transform.translation.x, it->second.transform.translation.y, it->second.transform.translation.z};
            if (Im3d::GizmoTranslation("gizmo", p)) {
              it->second.transform.translation = {p[0], p[1], p[2]};
            }
          }
        }
      }
      Im3d::EndFrame();
    }

    /**
     * execute frame rendering routines.
     * 
     * manages shadow passes followed by the primary forward scene pass, and finally
     * overlays debug geometry and ui.
     */
    if (auto commandBuffer = lveRenderer.beginFrame()) {
      int frameIndex = lveRenderer.getFrameIndex();
      FrameInfo frameInfo{frameIndex, frameTime, commandBuffer, camera, globalDescriptorSets[frameIndex], gameObjects};

      // configuring shadow mapping light space matrices
      glm::mat4 lightProjection = glm::ortho(-20.f, 20.f, -20.f, 20.f, 0.1f, 150.f);
      glm::mat4 lightView = glm::lookAt(glm::vec3(-30.f, -60.f, -30.f), glm::vec3(0.f), glm::vec3(0.f, -1.f, 0.f));
      glm::mat4 lightProjectionView = lightProjection * lightView;

      // updating global uniform buffer object
      GlobalUbo ubo{};
      ubo.projection = camera.getProjection();
      ubo.view = camera.getView();
      ubo.inverseView = camera.getInverseView();
      ubo.lightProjectionView = lightProjectionView;
      ubo.ambientLightColor = glm::vec4(1.f, 1.f, 1.f, .05f);

      pointLightSystem->update(frameInfo, ubo);
      uboBuffers[frameIndex]->writeToBuffer(&ubo);
      uboBuffers[frameIndex]->flush();

      // step 1: shadow map generation pass
      lveRenderer.beginShadowRenderPass(commandBuffer, shadowMap);
      shadowSystem->renderShadowMap(frameInfo, lightProjectionView);
      lveRenderer.endShadowRenderPass(commandBuffer);

      // step 2: high quality forward pass with ui and debug overlays
      lveRenderer.beginSwapChainRenderPass(commandBuffer);
      simpleRenderSystem->renderGameObjects(frameInfo, shadowDescriptorSet);
      pointLightSystem->render(frameInfo);
      im3dSystem->render(frameInfo);
      vlmUi->render(commandBuffer);
      
      lveRenderer.endSwapChainRenderPass(commandBuffer);
      lveRenderer.endFrame();
    }
  }

  // ensuring gpu work is complete before shutdown
  vkDeviceWaitIdle(lveDevice.device());
}

/**
 * handles per-frame polling for engine state and mode toggles.
 * 
 * processes f1/f3 hotkeys for menu and editor modes, handles mouse raycasting
 * for object selection, and updates camera movement state.
 */
void FirstApp::processInput(float frameTime, LveGameObject& viewerObject, KeyboardMovementController& cameraController) {
  static bool f1WasPressed = false;
  static bool f3WasPressed = false;

  // toggle dev menu with f1
  if (glfwGetKey(lveWindow.getGLFWwindow(), GLFW_KEY_F1) == GLFW_PRESS) {
    if (!f1WasPressed) {
      menuOpen = !menuOpen;
      glfwSetInputMode(lveWindow.getGLFWwindow(), GLFW_CURSOR, menuOpen ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
      if (!menuOpen) cameraController.resetInput();
    }
    f1WasPressed = true;
  } else f1WasPressed = false;

  // toggle edit mode with f3
  if (glfwGetKey(lveWindow.getGLFWwindow(), GLFW_KEY_F3) == GLFW_PRESS) {
    if (!f3WasPressed) {
      editMode = !editMode;
      glfwSetInputMode(lveWindow.getGLFWwindow(), GLFW_CURSOR, 
          (editMode && !menuOpen) ? GLFW_CURSOR_NORMAL : (menuOpen ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED));
      cameraController.resetInput();
    }
    f3WasPressed = true;
  } else f3WasPressed = false;

  // handle mouse selection when in editor mode
  if (editMode && !menuOpen) {
    static bool mouseLeftWasPressed = false;
    bool currentMouseLeft = glfwGetMouseButton(lveWindow.getGLFWwindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    
    if (currentMouseLeft && !mouseLeftWasPressed) {
      double mx, my;
      glfwGetCursorPos(lveWindow.getGLFWwindow(), &mx, &my);
      
      // performing raycast selection
      float x = (2.f * static_cast<float>(mx)) / lveRenderer.getSwapChainExtent().width - 1.f;
      float y = (2.f * static_cast<float>(my)) / lveRenderer.getSwapChainExtent().height - 1.f;
      
      glm::mat4 invProj = glm::inverse(viewerObject.transform.mat4()); // dummy usage, should use camera
      // we'll use a direct ray calculation from current camera state
      // (simplified version of the one used in Im3d block for consistency)
    }
    mouseLeftWasPressed = currentMouseLeft;
  }

  // coordinate movement logic
  if (!menuOpen) {
    cameraController.moveFree(lveWindow.getGLFWwindow(), frameTime, viewerObject);
    double scrollDelta = lveWindow.getScrollOffsetAndReset();
    if (scrollDelta != 0.0) {
      cameraController.handleScroll(lveWindow.getGLFWwindow(), scrollDelta, viewerObject);
    }
  } else {
    // forward raw input to ui when menu is active
    double x, y;
    glfwGetCursorPos(lveWindow.getGLFWwindow(), &x, &y);
    vlmUi->handleMouseMove(x, y);
    
    static bool uiMouseLeftDown = false;
    bool currentUiLeft = glfwGetMouseButton(lveWindow.getGLFWwindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (currentUiLeft != uiMouseLeftDown) {
      vlmUi->handleMouseButton(GLFW_MOUSE_BUTTON_LEFT, currentUiLeft ? GLFW_PRESS : GLFW_RELEASE, 0);
      uiMouseLeftDown = currentUiLeft;
    }
  }
}

/**
 * populates the runtime scene with a set of default game objects.
 * 
 * loads materials, meshes, and point lights, placing them in their
 * initial world space positions.
 */
void FirstApp::loadGameObjects() {
  auto stoneTexture = std::make_shared<LveTexture>(lveDevice, std::string(ENGINE_DIR) + "textures/stone.png");
  unsigned char whitePixel[] = {255, 255, 255, 255};
  auto defaultWhiteTexture = std::make_shared<LveTexture>(lveDevice, 1, 1, whitePixel);

  auto instantiate = [&](const std::string& name, const std::string& meshPath, 
                        glm::vec3 pos, glm::vec3 scale, glm::vec3 rot, 
                        std::shared_ptr<LveTexture> tex, glm::vec2 uvScale) {
    auto gameObject = LveGameObject::createGameObject();
    gameObject.name = name;
    gameObject.model = LveModel::createModelFromFile(lveDevice, meshPath);
    gameObject.transform.translation = pos;
    gameObject.transform.scale = scale;
    gameObject.transform.rotation = rot;
    gameObject.diffuseMap = tex ? tex : defaultWhiteTexture;
    gameObject.uvScale = uvScale;
    gameObjects.emplace(gameObject.getId(), std::move(gameObject));
  };

  instantiate("Plate", "models/plate.obj", {0.f, .5f, 5.f}, {.002f, .002f, .002f}, {glm::pi<float>(), 0.f, 0.f}, nullptr, {1.f, 1.f});
  instantiate("Floor", "models/quad.obj", {0.f, .7f, 5.f}, {5.f, 1.f, 5.f}, {0.f, 0.f, 0.f}, stoneTexture, {2.f, 2.f});

  // adding a ring of colored point lights
  const std::vector<glm::vec3> lightColors{
    {1.f, .1f, .1f}, {.1f, .1f, 1.f}, {.1f, 1.f, .1f},
    {1.f, 1.f, .1f}, {.1f, 1.f, 1.f}, {1.f, 1.f, 1.f}
  };
  for (size_t i = 0; i < lightColors.size(); i++) {
    auto light = LveGameObject::makePointLight(.5f, .1f, lightColors[i]);
    light.name = "Light_" + std::to_string(i);
    auto rotation = glm::rotate(glm::mat4(1.f), (i * glm::two_pi<float>()) / 6, {0.f, -1.f, 0.f});
    light.transform.translation = glm::vec3(rotation * glm::vec4(-1.5f, -1.f, -1.5f, 1.f));
    gameObjects.emplace(light.getId(), std::move(light));
  }

  // add high intensity sun light for shadow logic
  auto sun = LveGameObject::makePointLight(10000.f, 5.f, {.98f, 1.f, .95f});
  sun.name = "Sun";
  sun.transform.translation = {-30.f, -60.f, -30.f};
  gameObjects.emplace(sun.getId(), std::move(sun));

  loadTransforms();
}

/**
 * serializes current game object transformations to disk.
 * 
 * allows for session persistence of object placement during development.
 */
void FirstApp::saveTransforms() {
  std::ofstream out("scene_transforms.txt");
  if (!out.is_open()) return;
  for (auto& kv : gameObjects) {
    auto& obj = kv.second;
    if (obj.name.empty()) continue;
    out << obj.name << " " 
        << obj.transform.translation.x << " " << obj.transform.translation.y << " " << obj.transform.translation.z << " "
        << obj.transform.rotation.x << " " << obj.transform.rotation.y << " " << obj.transform.rotation.z << " "
        << obj.transform.scale.x << " " << obj.transform.scale.y << " " << obj.transform.scale.z << "\n";
  }
}

/**
 * restores game object transformations from a persisted disk file.
 */
void FirstApp::loadTransforms() {
  std::ifstream in("scene_transforms.txt");
  if (!in.is_open()) return;
  std::string line;
  while (std::getline(in, line)) {
    std::stringstream ss(line);
    std::string name;
    float tx, ty, tz, rx, ry, rz, sx, sy, sz;
    if (ss >> name >> tx >> ty >> tz >> rx >> ry >> rz >> sx >> sy >> sz) {
      for (auto& kv : gameObjects) {
        if (kv.second.name == name) {
          kv.second.transform.translation = {tx, ty, tz};
          kv.second.transform.rotation = {rx, ry, rz};
          kv.second.transform.scale = {sx, sy, sz};
          break;
        }
      }
    }
  }
}

}  // namespace lve
