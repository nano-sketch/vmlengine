#include "app/first_app.hpp"
#include "core/lve_window.hpp"
#include "core/lve_utils.hpp"
#include "input/keyboard_movement_controller.hpp"
#include "renderer/lve_buffer.hpp"
#include "renderer/lve_texture.hpp"
#include "systems/point_light_system.hpp"
#include "systems/simple_render_system.hpp"
#include "systems/im3d_system.hpp"
#include "scene/lve_camera.hpp"

#include <im3d.h>
#include <iostream>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cassert>
#include <chrono>
#include <stdexcept>
#include <numeric>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>

#undef max
#undef min

/**
 * first_app implementation.
 * orchestrates the main loop, resource orchestration, and rendering systems.
 */

namespace lve {

FirstApp::FirstApp() {
  globalPool = LveDescriptorPool::Builder(lveDevice)
                   .setMaxSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT + 100)
                   .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
                   .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100)
                   .build();
  loadGameObjects();
}

FirstApp::~FirstApp() { saveTransforms(); }

void FirstApp::run() {
  std::cout << "FirstApp::run() started" << std::endl;
  std::vector<std::unique_ptr<LveBuffer>> uboBuffers(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
  for (auto& buf : uboBuffers) {
    buf = std::make_unique<LveBuffer>(lveDevice, sizeof(GlobalUbo), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    buf->map();
  }
  std::cout << "UBO buffers created" << std::endl;

  auto globalSetLayout = LveDescriptorSetLayout::Builder(lveDevice).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS).build();
  std::vector<VkDescriptorSet> globalDescriptorSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
  for (size_t i = 0; i < globalDescriptorSets.size(); i++) {
    auto info = uboBuffers[i]->descriptorInfo();
    LveDescriptorWriter(*globalSetLayout, *globalPool) .writeBuffer(0, &info) .build(globalDescriptorSets[i]);
  }
  std::cout << "Descriptor sets created" << std::endl;

  const auto& extent = lveRenderer.getSwapChainExtent();
  vlmUi = std::make_unique<VlmUi>(lveDevice, lveRenderer.getSwapChainRenderPass(), extent.width, extent.height);
  std::cout << "VlmUi created" << std::endl;
  SimpleRenderSystem simpleRenderSystem{lveDevice, lveRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout()};
  std::cout << "SimpleRenderSystem created" << std::endl;
  PointLightSystem pointLightSystem{lveDevice, lveRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout()};
  std::cout << "PointLightSystem created" << std::endl;
  shadowMap = std::make_unique<LveShadowMap>(lveDevice, 2048, 2048);
  std::cout << "ShadowMap created" << std::endl;
  shadowSystem = std::make_unique<ShadowSystem>(lveDevice, lveRenderer.getShadowRenderPass());
  std::cout << "ShadowSystem created" << std::endl;
  Im3dSystem im3dSystem{lveDevice, lveRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout()};
  std::cout << "Im3dSystem created" << std::endl;

  {
    VkDescriptorImageInfo info{};
    info.sampler = shadowMap->getShadowSampler();
    info.imageView = shadowMap->getShadowImageView();
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    LveDescriptorWriter(simpleRenderSystem.getShadowSetLayout(), *globalPool).writeImage(0, &info).build(shadowDescriptorSet);
  }

  for (auto& kv : gameObjects) {
    auto& obj = kv.second;
    if (obj.diffuseMap) {
      VkDescriptorImageInfo info{};
      info.sampler = obj.diffuseMap->getSampler();
      info.imageView = obj.diffuseMap->getImageView();
      info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      
      LveDescriptorWriter(simpleRenderSystem.getTextureSetLayout(), *globalPool).writeImage(0, &info).build(obj.textureDescriptorSet);
    }
  }
      
  LveCamera camera{};
  auto viewerObject = LveGameObject::createGameObject();
  viewerObject.transform.translation.z = -2.5f;
  KeyboardMovementController cameraController{};

  auto currentTime = std::chrono::high_resolution_clock::now();
  bool f1P = false, f3P = false;
  float perfT = 0.0f;
  int fCount = 0;

  std::cout << "Entering main loop" << std::endl;
  while (!lveWindow.shouldClose()) {
    glfwPollEvents();
    auto newTime = std::chrono::high_resolution_clock::now();
    float frameTime = std::min(std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count(), 0.1f);
    currentTime = newTime;

    if (glfwGetKey(lveWindow.getGLFWwindow(), GLFW_KEY_F1) == GLFW_PRESS) {
      if (!f1P) {
        menuOpen = !menuOpen;
        glfwSetInputMode(lveWindow.getGLFWwindow(), GLFW_CURSOR, menuOpen ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
        if (!menuOpen) cameraController.resetInput();
      }
      f1P = true;
    } else f1P = false;

    if (glfwGetKey(lveWindow.getGLFWwindow(), GLFW_KEY_F3) == GLFW_PRESS) {
      if (!f3P) {
        editMode = !editMode;
        glfwSetInputMode(lveWindow.getGLFWwindow(), GLFW_CURSOR, (editMode && !menuOpen) ? GLFW_CURSOR_NORMAL : (menuOpen ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED));
        cameraController.resetInput();
      }
      f3P = true;
    } else f3P = false;

    if (editMode && !menuOpen) {
      static bool lmP = false;
      bool curLP = glfwGetMouseButton(lveWindow.getGLFWwindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
      if (curLP && !lmP) {
        double mx, my; glfwGetCursorPos(lveWindow.getGLFWwindow(), &mx, &my);
        float x = (2.f * static_cast<float>(mx)) / lveRenderer.getSwapChainExtent().width - 1.f;
        float y = (2.f * static_cast<float>(my)) / lveRenderer.getSwapChainExtent().height - 1.f;
        glm::vec4 rClip = {x, y, 0.1f, 1.f};
        glm::vec4 rEye = glm::inverse(camera.getProjection()) * rClip;
        rEye = {rEye.x, rEye.y, 1.f, 0.f};
        glm::vec3 rWorld = glm::normalize(glm::vec3(camera.getInverseView() * rEye));
        glm::vec3 rOrig = camera.getPosition();
        
        float min_t = std::numeric_limits<float>::max();
        LveGameObject::id_t hitId = std::numeric_limits<LveGameObject::id_t>::max();
        for (auto& kv : gameObjects) {
          auto& obj = kv.second;
          if (!obj.model) continue;
          glm::mat4 invMat = glm::inverse(obj.transform.mat4());
          glm::vec3 lO = glm::vec3(invMat * glm::vec4(rOrig, 1.f));
          glm::vec3 lD = glm::normalize(glm::vec3(invMat * glm::vec4(rWorld, 0.f)));
          const auto& bbox = obj.model->getBoundingBox();
          
          float tx1 = (bbox.min.x - lO.x) / lD.x, tx2 = (bbox.max.x - lO.x) / lD.x;
          float tmin = std::min(tx1, tx2), tmax = std::max(tx1, tx2);
          
          float ty1 = (bbox.min.y - lO.y) / lD.y, ty2 = (bbox.max.y - lO.y) / lD.y;
          tmin = std::max(tmin, std::min(ty1, ty2));
          tmax = std::min(tmax, std::max(ty1, ty2));
          
          float tz1 = (bbox.min.z - lO.z) / lD.z, tz2 = (bbox.max.z - lO.z) / lD.z;
          tmin = std::max(tmin, std::min(tz1, tz2));
          tmax = std::min(tmax, std::max(tz1, tz2));

          if (tmax >= tmin && tmax > 0 && tmin < min_t) { min_t = tmin; hitId = obj.getId(); }
        }
        selectedObjectId = hitId;
      }
      lmP = curLP;
    }

    if (!menuOpen) {
      cameraController.moveFree(lveWindow.getGLFWwindow(), frameTime, viewerObject);
      double sOff = lveWindow.getScrollOffsetAndReset();
      if (sOff != 0.0) cameraController.handleScroll(lveWindow.getGLFWwindow(), sOff, viewerObject);
    } else {
      double x, y; glfwGetCursorPos(lveWindow.getGLFWwindow(), &x, &y);
      vlmUi->handleMouseMove(x, y);
      static bool lD = false;
      bool curLD = glfwGetMouseButton(lveWindow.getGLFWwindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
      if (curLD != lD) { vlmUi->handleMouseButton(GLFW_MOUSE_BUTTON_LEFT, curLD ? GLFW_PRESS : GLFW_RELEASE, 0); lD = curLD; }
    }

    vlmUi->update();
    auto cp = camera.getPosition();
    camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);
    
    perfT += frameTime; fCount++;
    if (perfT >= 0.2f) { vlmUi->updateTelemetry(fCount / perfT, cp.x, cp.y, cp.z); perfT = 0.f; fCount = 0; }
    
    float aspect = lveRenderer.getAspectRatio();
    camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);
    
    auto currentExtent = lveRenderer.getSwapChainExtent();
    if (vlmUi) vlmUi->resize(currentExtent.width, currentExtent.height);
    
    {
      auto& ad = Im3d::GetAppData(); ad.m_deltaTime = frameTime;
      ad.m_viewportSize = {static_cast<float>(currentExtent.width), static_cast<float>(currentExtent.height)};
      ad.m_viewOrigin = {cp.x, cp.y, cp.z};
      double mx, my; glfwGetCursorPos(lveWindow.getGLFWwindow(), &mx, &my);
      float nx = (2.f*static_cast<float>(mx))/currentExtent.width-1.f, ny = (2.f*static_cast<float>(my))/currentExtent.height-1.f;
      glm::vec4 rc = {nx, ny, 0.1f, 1.f}; glm::vec4 re = glm::inverse(camera.getProjection()) * rc; re = {re.x, re.y, 1.f, 0.f};
      glm::vec3 rw = glm::normalize(glm::vec3(camera.getInverseView() * re));
      ad.m_cursorRayOrigin = {cp.x, cp.y, cp.z}; ad.m_cursorRayDirection = {rw.x, rw.y, rw.z};
      ad.m_keyDown[Im3d::Mouse_Left] = glfwGetMouseButton(lveWindow.getGLFWwindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
      ad.m_projScaleY = tanf(glm::radians(50.f) * 0.5f) * 2.0f;
      Im3d::NewFrame();
      if (editMode) {
        for (auto& kv : gameObjects) {
          auto& obj = kv.second; if (!obj.model) continue;
          bool sel = (obj.getId() == selectedObjectId);
          Im3d::PushColor(sel ? Im3d::Color_Cyan : Im3d::Color_Yellow); Im3d::PushSize(sel ? 2.f : 1.f);
          Im3d::PushMatrix(Im3dSystem::toIm3d(obj.transform.mat4()));
          const auto& b = obj.model->getBoundingBox(); Im3d::DrawAlignedBox({b.min.x, b.min.y, b.min.z}, {b.max.x, b.max.y, b.max.z});
          Im3d::PopMatrix(); Im3d::PopSize(); Im3d::PopColor();
        }
        if (selectedObjectId != std::numeric_limits<LveGameObject::id_t>::max()) {
          auto it = gameObjects.find(selectedObjectId);
          if (it != gameObjects.end()) {
            float p[3] = {it->second.transform.translation.x, it->second.transform.translation.y, it->second.transform.translation.z};
            if (Im3d::GizmoTranslation("gizmo", p)) it->second.transform.translation = {p[0], p[1], p[2]};
          }
        }
      }
      Im3d::EndFrame();
    }
    
    if (auto cmd = lveRenderer.beginFrame()) {
      int idx = lveRenderer.getFrameIndex();
      FrameInfo info{idx, frameTime, cmd, camera, globalDescriptorSets[idx], gameObjects};
      glm::mat4 lp = glm::ortho(-20.f, 20.f, -20.f, 20.f, 0.1f, 150.f);
      glm::mat4 lv = glm::lookAt(glm::vec3(-30.f,-60.f,-30.f), glm::vec3(0.f), glm::vec3(0.f,-1.f,0.f));
      glm::mat4 lpv = lp * lv;
      
      GlobalUbo ubo;
      ubo.projection = camera.getProjection();
      ubo.view = camera.getView();
      ubo.inverseView = camera.getInverseView();
      ubo.lightProjectionView = lpv;
      ubo.ambientLightColor = glm::vec4(1.f, 1.f, 1.f, .05f);
      
      pointLightSystem.update(info, ubo);
      uboBuffers[idx]->writeToBuffer(&ubo); uboBuffers[idx]->flush();

      lveRenderer.beginShadowRenderPass(cmd, shadowMap);
      shadowSystem->renderShadowMap(info, lpv);
      lveRenderer.endShadowRenderPass(cmd);

      lveRenderer.beginSwapChainRenderPass(cmd);
      simpleRenderSystem.renderGameObjects(info, shadowDescriptorSet);
      pointLightSystem.render(info);
      im3dSystem.render(info);
      vlmUi->render(cmd);
      lveRenderer.endSwapChainRenderPass(cmd);
      lveRenderer.endFrame();
    }
  }
  vkDeviceWaitIdle(lveDevice.device());
}

void FirstApp::loadGameObjects() {
  auto tex = std::make_shared<LveTexture>(lveDevice, std::string(ENGINE_DIR) + "textures/stone.png");
  unsigned char white[] = {255, 255, 255, 255};
  auto whiteT = std::make_shared<LveTexture>(lveDevice, 1, 1, white);

  auto load = [&](const std::string& n, const std::string& p, glm::vec3 t, glm::vec3 s, glm::vec3 r, std::shared_ptr<LveTexture> tx, glm::vec2 uv) {
    auto obj = LveGameObject::createGameObject();
    obj.name = n; obj.model = LveModel::createModelFromFile(lveDevice, p); obj.transform.translation = t; obj.transform.scale = s; obj.transform.rotation = r; obj.diffuseMap = tx ? tx : whiteT; obj.uvScale = uv;
    gameObjects.emplace(obj.getId(), std::move(obj));
  };

  load("Plate", "models/plate.obj", {0.f, .5f, 5.f}, {.002f, .002f, .002f}, {glm::pi<float>(), 0.f, 0.f}, nullptr, {1.f, 1.f});
  load("Floor", "models/quad.obj", {0.f, .7f, 5.f}, {5.f, 1.f, 5.f}, {0.f, 0.f, 0.f}, tex, {2.f, 2.f});

  const std::vector<glm::vec3> colors{{1.f, .1f, .1f}, {.1f, .1f, 1.f}, {.1f, 1.f, .1f}, {1.f, 1.f, .1f}, {.1f, 1.f, 1.f}, {1.f, 1.f, 1.f}};
  for (size_t i = 0; i < colors.size(); i++) {
    auto l = LveGameObject::makePointLight(.5f, .1f, colors[i]);
    l.name = "Light_" + std::to_string(i);
    auto rot = glm::rotate(glm::mat4(1.f), (i * glm::two_pi<float>()) / 6, {0.f, -1.f, 0.f});
    l.transform.translation = glm::vec3(rot * glm::vec4(-1.5f, -1.f, -1.5f, 1.f));
    gameObjects.emplace(l.getId(), std::move(l));
  }

  auto sun = LveGameObject::makePointLight(10000.f, 5.f, {.98f, 1.f, .95f});
  sun.name = "Sun"; sun.transform.translation = {-30.f, -60.f, -30.f};
  gameObjects.emplace(sun.getId(), std::move(sun));
  loadTransforms();
}

void FirstApp::saveTransforms() {
  std::ofstream f("scene_transforms.txt"); if (!f.is_open()) return;
  for (auto& kv : gameObjects) {
    auto& o = kv.second; if (o.name.empty()) continue;
    f << o.name << " " << o.transform.translation.x << " " << o.transform.translation.y << " " << o.transform.translation.z << " " << o.transform.rotation.x << " " << o.transform.rotation.y << " " << o.transform.rotation.z << " " << o.transform.scale.x << " " << o.transform.scale.y << " " << o.transform.scale.z << "\n";
  }
}

void FirstApp::loadTransforms() {
  std::ifstream f("scene_transforms.txt"); if (!f.is_open()) return;
  std::string l;
  while (std::getline(f, l)) {
    std::stringstream ss(l); std::string n; float tx, ty, tz, rx, ry, rz, sx, sy, sz;
    if (ss >> n >> tx >> ty >> tz >> rx >> ry >> rz >> sx >> sy >> sz) {
      for (auto& kv : gameObjects) { if (kv.second.name == n) { kv.second.transform.translation = {tx, ty, tz}; kv.second.transform.rotation = {rx, ry, rz}; kv.second.transform.scale = {sx, sy, sz}; break; } }
    }
  }
}

}  // namespace lve
