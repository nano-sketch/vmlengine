#include "ui/vlm_ui.hpp"
#include <AppCore/CAPI.h>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cstdio>

/**
 * ui implementation.
 * manages the lifecycle of the ultralight renderer and synchronizes browser frames to vulkan textures.
 */

namespace lve {

static void onConsoleMessage(void* data, ULView caller, ULMessageSource src, ULMessageLevel lvl, ULString msg, unsigned int line, unsigned int col, ULString src_id) {
  std::cout << "ui console: " << ulStringGetData(msg) << " (line " << line << ")" << std::endl;
}

VlmUi::VlmUi(LveDevice &device, VkRenderPass rp, uint32_t w, uint32_t h)
    : lveDevice{device}, currentRenderPass{rp}, width{w}, height{h} {

  std::cout << "VlmUi constructor started" << std::endl;
  config = ulCreateConfig();
  std::cout << "ulCreateConfig done" << std::endl;
  ULString resPath = ulCreateString("resources/");
  ulConfigSetResourcePathPrefix(config, resPath);
  ulDestroyString(resPath);
  
  ULString baseDir = ulCreateString("./");
  ulEnablePlatformFontLoader();
  ulEnablePlatformFileSystem(baseDir);
  ulDestroyString(baseDir);
  std::cout << "ulEnablePlatformFileSystem done" << std::endl;

  renderer = ulCreateRenderer(config);
  std::cout << "ulCreateRenderer done" << std::endl;

  ULViewConfig viewConfig = ulCreateViewConfig();
  ulViewConfigSetIsTransparent(viewConfig, true);
  ulViewConfigSetInitialFocus(viewConfig, true);

  int winW, winH, fbW, fbH;
  GLFWwindow* glfwWin = lveDevice.getWindow().getGLFWwindow();
  glfwGetWindowSize(glfwWin, &winW, &winH);
  glfwGetFramebufferSize(glfwWin, &fbW, &fbH);
  ulViewConfigSetInitialDeviceScale(viewConfig, (double)fbW / (double)winW);
  std::cout << "View config created" << std::endl;

  view = ulCreateView(renderer, width, height, viewConfig, nullptr);
  std::cout << "ulCreateView done" << std::endl;
  ulDestroyViewConfig(viewConfig);
  ulViewFocus(view);
  ulViewSetAddConsoleMessageCallback(view, onConsoleMessage, nullptr);

  ULString htmlStr = ulCreateString(R"html(
    <html>
      <head>
        <style>
          :root {
            --bg: rgba(10, 10, 15, 0.85);
            --accent: #00f2ff;
            --border: rgba(255, 255, 255, 0.12);
            --text-main: #ffffff;
            --text-dim: rgba(255, 255, 255, 0.5);
          }
          body { 
            margin: 0; padding: 0; background: transparent; overflow: hidden; 
            font-family: -apple-system, "Segoe UI", Roboto, sans-serif; 
          }
          .hud {
            position: absolute; top: 30px; left: 30px;
            width: 240px; background: var(--bg);
            border: 1px solid var(--border); border-radius: 14px;
            color: var(--text-main); padding: 18px;
            box-shadow: 0 10px 40px rgba(0, 0, 0, 0.6);
            backdrop-filter: blur(12px);
            user-select: none; transition: opacity 0.3s, transform 0.3s;
          }
          .header {
            display: flex; align-items: center; margin-bottom: 20px; cursor: move;
            border-bottom: 1px solid var(--border); padding-bottom: 10px;
          }
          .title { font-size: 10px; font-weight: 800; letter-spacing: 0.15em; color: var(--text-dim); text-transform: uppercase; }
          .stat-item { margin-bottom: 14px; }
          .stat-item:last-child { margin-bottom: 0; }
          .label { font-size: 9px; font-weight: 600; color: var(--text-dim); text-transform: uppercase; letter-spacing: 0.05em; margin-bottom: 2px; }
          .value { font-size: 15px; font-weight: 700; color: var(--accent); font-family: "JetBrains Mono", monospace; }
        </style>
      </head>
      <body>
        <div class="hud" id="dragBox">
          <div class="header" id="handle">
            <div class="title">VML Engine Runtime</div>
          </div>
          <div class="stat-item">
            <div class="label">Renderer</div>
            <div class="value" style="color: #fff">Vulkan 1.3 / HLSL</div>
          </div>
          <div class="stat-item">
            <div class="label">Framerate</div>
            <div class="value" id="fps_val">0.0 FPS</div>
          </div>
          <div class="stat-item">
            <div class="label">Coordinates (XYZ)</div>
            <div class="value" id="pos_val">0.0, 0.0, 0.0</div>
          </div>
          <div class="stat-item">
            <div class="label">Tick Count</div>
            <div class="value" id="cycle_val">0</div>
          </div>
        </div>
        <script>
          const box = document.getElementById('dragBox'), handle = document.getElementById('handle');
          let isDragging = false, ox, oy;
          handle.onmousedown = (e) => { isDragging = true; ox = e.clientX - box.offsetLeft; oy = e.clientY - box.offsetTop; box.style.borderColor = 'rgba(0, 242, 255, 0.4)'; };
          window.onmousemove = (e) => {
            if (!isDragging) return;
            let x = e.clientX - ox, y = e.clientY - oy;
            box.style.left = x + 'px'; box.style.top = y + 'px';
          };
          window.onmouseup = () => { isDragging = false; box.style.borderColor = 'rgba(255, 255, 255, 0.12)'; };
          window.updateTelemetry = (fps, x, y, z) => {
            document.getElementById('fps_val').innerText = `${fps.toFixed(1)} FPS`;
            document.getElementById('pos_val').innerText = `${x.toFixed(2)}, ${y.toFixed(2)}, ${z.toFixed(2)}`;
          };
          let c = 0; setInterval(() => { document.getElementById('cycle_val').innerText = c++; }, 100);
        </script>
      </body>
    </html>
  )html");
  ulViewLoadHTML(view, htmlStr);
  ulDestroyString(htmlStr);
  std::cout << "HTML loaded" << std::endl;

  createUiTexture();
  std::cout << "createUiTexture done" << std::endl;
  createPipeline(currentRenderPass);
  std::cout << "createPipeline done" << std::endl;
}

VlmUi::~VlmUi() {
  vkDestroySampler(lveDevice.device(), uiSampler, nullptr);
  vkDestroyImageView(lveDevice.device(), uiImageView, nullptr);
  vkDestroyImage(lveDevice.device(), uiImage, nullptr);
  vkFreeMemory(lveDevice.device(), uiImageMemory, nullptr);
  vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);

  ulDestroyView(view);
  ulDestroyRenderer(renderer);
  ulDestroyConfig(config);
}

void VlmUi::resize(uint32_t nw, uint32_t nh) {
  if (nw == width && nh == height) return;
  width = nw; height = nh;
  ulViewResize(view, width, height);

  int winW, winH, fbW, fbH;
  GLFWwindow* glfwWin = lveDevice.getWindow().getGLFWwindow();
  glfwGetWindowSize(glfwWin, &winW, &winH);
  glfwGetFramebufferSize(glfwWin, &fbW, &fbH);
  ulViewSetDeviceScale(view, (double)fbW / (double)winW);

  vkDestroySampler(lveDevice.device(), uiSampler, nullptr);
  vkDestroyImageView(lveDevice.device(), uiImageView, nullptr);
  vkDestroyImage(lveDevice.device(), uiImage, nullptr);
  vkFreeMemory(lveDevice.device(), uiImageMemory, nullptr);
  createUiTexture();
}

void VlmUi::update() {
  ulRefreshDisplay(renderer, 0);
  ulUpdate(renderer);
  ulRender(renderer);

  ULSurface surface = ulViewGetSurface(view);
  ULIntRect dirty = ulSurfaceGetDirtyBounds(surface);
  if (dirty.right > dirty.left && dirty.bottom > dirty.top) {
    updateUiTexture();
    ulSurfaceClearDirtyBounds(surface);
  }
}

void VlmUi::updateTelemetry(float fps, float x, float y, float z) {
  char cmd[256];
  snprintf(cmd, sizeof(cmd), "updateTelemetry(%f, %f, %f, %f)", fps, x, y, z);
  ULString script = ulCreateString(cmd);
  ulViewEvaluateScript(view, script, nullptr);
  ulDestroyString(script);
}

void VlmUi::render(VkCommandBuffer cmd) {
  lvePipeline->bind(cmd);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
  vkCmdDraw(cmd, 3, 1, 0, 0);
}

void VlmUi::createUiTexture() {
  VkImageCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.imageType = VK_IMAGE_TYPE_2D;
  info.format = VK_FORMAT_B8G8R8A8_UNORM;
  info.extent = {width, height, 1};
  info.mipLevels = 1;
  info.arrayLayers = 1;
  info.samples = VK_SAMPLE_COUNT_1_BIT;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  
  lveDevice.createImageWithInfo(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, uiImage, uiImageMemory);

  auto cmd = lveDevice.beginSingleTimeCommands();
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = uiImage;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = 0;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
  lveDevice.endSingleTimeCommands(cmd);

  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = uiImage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;
  
  if (vkCreateImageView(lveDevice.device(), &viewInfo, nullptr, &uiImageView) != VK_SUCCESS) throw std::runtime_error("failed to create ui image view");

  VkSamplerCreateInfo sampInfo{};
  sampInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampInfo.magFilter = VK_FILTER_LINEAR;
  sampInfo.minFilter = VK_FILTER_LINEAR;
  sampInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampInfo.mipLodBias = 0.f;
  sampInfo.anisotropyEnable = VK_FALSE;
  sampInfo.compareEnable = VK_FALSE;
  sampInfo.minLod = 0.f;
  sampInfo.maxLod = 1.f;
  sampInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  
  if (vkCreateSampler(lveDevice.device(), &sampInfo, nullptr, &uiSampler) != VK_SUCCESS) throw std::runtime_error("failed to create ui sampler");

  descriptorPool = LveDescriptorPool::Builder(lveDevice).setMaxSets(1).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1).build();
  descriptorSetLayout = LveDescriptorSetLayout::Builder(lveDevice).addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT).build();
  VkDescriptorImageInfo imageInfo{uiSampler, uiImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  LveDescriptorWriter(*descriptorSetLayout, *descriptorPool).writeImage(0, &imageInfo).build(descriptorSet);

  stagingBuffer = std::make_unique<LveBuffer>(lveDevice, 4, width * height, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  stagingBuffer->map();
}

void VlmUi::updateUiTexture() {
  ULSurface surf = ulViewGetSurface(view);
  ULBitmap bmp = ulBitmapSurfaceGetBitmap(surf);
  void* px = ulBitmapLockPixels(bmp);
  stagingBuffer->writeToBuffer(px);
  ulBitmapUnlockPixels(bmp);

  auto cmd = lveDevice.beginSingleTimeCommands();
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = uiImage;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
  lveDevice.copyBufferToImage(stagingBuffer->getBuffer(), uiImage, width, height, 1);
  
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
  lveDevice.endSingleTimeCommands(cmd);
}

void VlmUi::createPipeline(VkRenderPass rp) {
  VkPipelineLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  info.setLayoutCount = 1;
  VkDescriptorSetLayout layouts[] = {descriptorSetLayout->getDescriptorSetLayout()};
  info.pSetLayouts = layouts;
  if (vkCreatePipelineLayout(lveDevice.device(), &info, nullptr, &pipelineLayout) != VK_SUCCESS) throw std::runtime_error("failed to create ui pipeline layout");

  PipelineConfigInfo config{};
  LvePipeline::defaultPipelineConfigInfo(config);
  config.renderPass = rp;
  config.pipelineLayout = pipelineLayout;
  
  config.colorBlendAttachment.blendEnable = VK_TRUE;
  config.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  config.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  config.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  config.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  config.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  config.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
  config.colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  
  config.depthStencilInfo.depthTestEnable = VK_FALSE;
  config.depthStencilInfo.depthWriteEnable = VK_FALSE;
  lvePipeline = std::make_unique<LvePipeline>(lveDevice, "shaders/ui.vert.spv", "shaders/ui.frag.spv", config);
}

void VlmUi::handleMouseMove(double x, double y) {
  bool isDown = glfwGetMouseButton(lveDevice.getWindow().getGLFWwindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
  ULMouseEvent evt = ulCreateMouseEvent(kMouseEventType_MouseMoved, (int)x, (int)y, isDown ? kMouseButton_Left : kMouseButton_None);
  ulViewFireMouseEvent(view, evt);
  ulDestroyMouseEvent(evt);
}

void VlmUi::handleMouseButton(int btn, int act, int mods) {
  ULMouseButton ubtn = (btn == 0) ? kMouseButton_Left : (btn == 1 ? kMouseButton_Right : (btn == 2 ? kMouseButton_Middle : kMouseButton_None));
  double x, y; glfwGetCursorPos(lveDevice.getWindow().getGLFWwindow(), &x, &y);
  ULMouseEvent evt = ulCreateMouseEvent((act == 1) ? kMouseEventType_MouseDown : kMouseEventType_MouseUp, (int)x, (int)y, ubtn);
  ulViewFireMouseEvent(view, evt);
  ulDestroyMouseEvent(evt);
}

}  // namespace lve
