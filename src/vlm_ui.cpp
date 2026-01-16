#include "vlm_ui.hpp"

// libs
#include <AppCore/CAPI.h>

// std
#include <iostream>
#include <stdexcept>
#include <cstring>

namespace lve {

// JS Console Callback
void onConsoleMessage(void* user_data, ULView caller, ULMessageSource source,
                      ULMessageLevel level, ULString message,
                      unsigned int line_number, unsigned int column_number,
                      ULString source_id) {
  const char* msg = ulStringGetData(message);
  std::cout << "[UI Console] " << msg << " (line " << line_number << ")" << std::endl;
}

VlmUi::VlmUi(LveDevice &device, VkRenderPass renderPass, uint32_t width, uint32_t height)
    : lveDevice{device}, renderPass{renderPass}, width{width}, height{height} {
  
  // 1. Ultralight Setup
  config = ulCreateConfig();
  ULString resPath = ulCreateString("resources/");
  ulConfigSetResourcePathPrefix(config, resPath);
  ulDestroyString(resPath);
  
  ULString baseDir = ulCreateString("./");
  ulEnablePlatformFontLoader();
  ulEnablePlatformFileSystem(baseDir);
  ulDestroyString(baseDir);

  renderer = ulCreateRenderer(config);

  ULViewConfig viewConfig = ulCreateViewConfig();
  ulViewConfigSetIsTransparent(viewConfig, true);
  ulViewConfigSetInitialFocus(viewConfig, true);
  
  // High DPI Support
  int winW, winH, fbW, fbH;
  GLFWwindow* glfwWin = lveDevice.getWindow().getGLFWwindow();
  glfwGetWindowSize(glfwWin, &winW, &winH);
  glfwGetFramebufferSize(glfwWin, &fbW, &fbH);
  double scale = (double)fbW / (double)winW;
  ulViewConfigSetInitialDeviceScale(viewConfig, scale);

  view = ulCreateView(renderer, width, height, viewConfig, nullptr);
  ulDestroyViewConfig(viewConfig);
  ulViewFocus(view);
  ulViewSetAddConsoleMessageCallback(view, onConsoleMessage, nullptr);

  // Load a simple HTML with an ImGui-style look and auto-docking
  ULString htmlStr = ulCreateString(R"html(
    <html>
      <head>
        <style>
          body { 
            margin: 0; padding: 0; 
            background: transparent; 
            overflow: hidden; 
            font-family: -apple-system, "Segoe UI", Roboto, sans-serif;
            font-size: 13px;
          }
          .window {
            position: absolute;
            top: 50px; left: 50px;
            width: 300px;
            background: #0f0f0f;
            border: 1px solid #333333;
            color: #eeeeee;
            display: flex; flex-direction: column;
            box-shadow: 2px 2px 10px rgba(0,0,0,0.5);
            user-select: none;
            transition: left 0.15s ease-out, top 0.15s ease-out;
          }
          .titlebar {
            padding: 4px;
            background: #004488;
            height: 12px;
            cursor: move;
          }
          .content { padding: 12px; flex: 1; }
          .item { display: flex; margin-bottom: 8px; align-items: baseline; }
          .label { width: 80px; color: #888888; font-size: 10px; text-transform: uppercase; font-weight: bold; }
          .value { color: #00ffca; font-family: monospace; font-weight: bold; font-size: 14px; }
        </style>
      </head>
      <body>
        <div class="window" id="dragBox">
          <div class="titlebar" id="handle"></div>
          <div class="content">
            <div class="item"><span class="label">GPU:</span><span class="value">Vulkan v1.3</span></div>
            <div class="item"><span class="label">FPS:</span><span id="fps_val" class="value">0.0</span></div>
            <div class="item"><span class="label">POS:</span><span id="pos_val" class="value">0.0, 0.0, 0.0</span></div>
            <div class="item"><span class="label">Cycle:</span><span id="cycle_val" class="value">0</span></div>
          </div>
        </div>
        <script>
          const box = document.getElementById('dragBox');
          const handle = document.getElementById('handle');
          let isDragging = false;
          let offX, offY;
          const SNAP_THRESHOLD = 50; 
          const PADDING = 15;

          handle.onmousedown = function(e) {
            isDragging = true;
            offX = e.clientX - box.offsetLeft;
            offY = e.clientY - box.offsetTop;
            box.style.transition = 'none';
          };

          window.onmousemove = function(e) {
            if (isDragging) {
              let newX = e.clientX - offX;
              let newY = e.clientY - offY;
              
              const screenW = window.innerWidth;
              const screenH = window.innerHeight;
              const boxW = box.offsetWidth;
              const boxH = box.offsetHeight;

              // Snap logic
              if (newX < SNAP_THRESHOLD) newX = PADDING;
              else if (newX + boxW > screenW - SNAP_THRESHOLD) newX = screenW - boxW - PADDING;

              if (newY < SNAP_THRESHOLD) newY = PADDING;
              else if (newY + boxH > screenH - SNAP_THRESHOLD) newY = screenH - boxH - PADDING;

              box.style.left = newX + 'px';
              box.style.top = newY + 'px';
            }
          };

          window.onmouseup = function() { 
            isDragging = false; 
            box.style.transition = 'left 0.15s ease-out, top 0.15s ease-out';
          };

          // Bridge for C++
          window.updateTelemetry = function(fps, x, y, z) {
            document.getElementById('fps_val').innerText = fps.toFixed(1);
            document.getElementById('pos_val').innerText = x.toFixed(1) + ', ' + y.toFixed(1) + ', ' + z.toFixed(1);
          };

          let count = 0;
          setInterval(() => {
            document.getElementById('cycle_val').innerText = count++;
          }, 100);
        </script>
      </body>
    </html>
  )html");
  ulViewLoadHTML(view, htmlStr);
  ulDestroyString(htmlStr);

  // 2. Vulkan Setup
  createUiTexture();
  createPipeline(renderPass);
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

void VlmUi::resize(uint32_t newWidth, uint32_t newHeight) {
  if (newWidth == width && newHeight == height) return;
  width = newWidth;
  height = newHeight;

  ulViewResize(view, width, height);
  
  // Update Scale for High DPI
  int winW, winH, fbW, fbH;
  GLFWwindow* glfwWin = lveDevice.getWindow().getGLFWwindow();
  glfwGetWindowSize(glfwWin, &winW, &winH);
  glfwGetFramebufferSize(glfwWin, &fbW, &fbH);
  double scale = (double)fbW / (double)winW;
  ulViewSetDeviceScale(view, scale);

  // Clean old Vulkan resources
  vkDestroySampler(lveDevice.device(), uiSampler, nullptr);
  vkDestroyImageView(lveDevice.device(), uiImageView, nullptr);
  vkDestroyImage(lveDevice.device(), uiImage, nullptr);
  vkFreeMemory(lveDevice.device(), uiImageMemory, nullptr);
  
  // Recreate with new size
  createUiTexture();
  // We don't need to recreate the pipeline, just update the texture
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

void VlmUi::render(VkCommandBuffer commandBuffer) {
  lvePipeline->bind(commandBuffer);

  vkCmdBindDescriptorSets(
      commandBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipelineLayout,
      0,
      1,
      &descriptorSet,
      0,
      nullptr);

  vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

void VlmUi::createUiTexture() {
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

  lveDevice.createImageWithInfo(
      imageInfo,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      uiImage,
      uiImageMemory);

  // Transition to shader read
  VkCommandBuffer commandBuffer = lveDevice.beginSingleTimeCommands();
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

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
  lveDevice.endSingleTimeCommands(commandBuffer);

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

  if (vkCreateImageView(lveDevice.device(), &viewInfo, nullptr, &uiImageView) != VK_SUCCESS) {
    throw std::runtime_error("failed to create ui image view!");
  }

  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.anisotropyEnable = VK_FALSE;
  samplerInfo.maxAnisotropy = 1.0f;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

  if (vkCreateSampler(lveDevice.device(), &samplerInfo, nullptr, &uiSampler) != VK_SUCCESS) {
    throw std::runtime_error("failed to create ui sampler!");
  }

  descriptorPool = LveDescriptorPool::Builder(lveDevice)
      .setMaxSets(1)
      .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
      .build();

  descriptorSetLayout = LveDescriptorSetLayout::Builder(lveDevice)
      .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
      .build();

  auto imageInfoDescriptor = VkDescriptorImageInfo{uiSampler, uiImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  LveDescriptorWriter(*descriptorSetLayout, *descriptorPool)
      .writeImage(0, &imageInfoDescriptor)
      .build(descriptorSet);

  stagingBuffer = std::make_unique<LveBuffer>(
      lveDevice, 4, width * height,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  stagingBuffer->map();
}

void VlmUi::updateUiTexture() {
  ULSurface surface = ulViewGetSurface(view);
  ULBitmap bitmap = ulBitmapSurfaceGetBitmap(surface);
  void* pixels = ulBitmapLockPixels(bitmap);
  stagingBuffer->writeToBuffer(pixels);
  ulBitmapUnlockPixels(bitmap);

  VkCommandBuffer commandBuffer = lveDevice.beginSingleTimeCommands();
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

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
  lveDevice.copyBufferToImage(stagingBuffer->getBuffer(), uiImage, width, height, 1);

  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

  lveDevice.endSingleTimeCommands(commandBuffer);
}

void VlmUi::createPipeline(VkRenderPass renderPass) {
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  VkDescriptorSetLayout layouts[] = {descriptorSetLayout->getDescriptorSetLayout()};
  pipelineLayoutInfo.pSetLayouts = layouts;
  
  if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create ui pipeline layout!");
  }

  PipelineConfigInfo pipelineConfig{};
  LvePipeline::defaultPipelineConfigInfo(pipelineConfig);
  pipelineConfig.renderPass = renderPass;
  pipelineConfig.pipelineLayout = pipelineLayout;
  pipelineConfig.colorBlendAttachment.blendEnable = VK_TRUE;
  pipelineConfig.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  pipelineConfig.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  pipelineConfig.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  pipelineConfig.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  pipelineConfig.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  pipelineConfig.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
  
  pipelineConfig.depthStencilInfo.depthTestEnable = VK_FALSE;
  pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;

  lvePipeline = std::make_unique<LvePipeline>(lveDevice, "shaders/ui.vert.spv", "shaders/ui.frag.spv", pipelineConfig);
}

void VlmUi::handleMouseMove(double x, double y) {
  GLFWwindow* window = lveDevice.getWindow().getGLFWwindow();

  bool isLeftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
  ULMouseButton btn = isLeftDown ? kMouseButton_Left : kMouseButton_None;

  ULMouseEvent evt = ulCreateMouseEvent(kMouseEventType_MouseMoved, (int)x, (int)y, btn);
  ulViewFireMouseEvent(view, evt);
  ulDestroyMouseEvent(evt);
}

void VlmUi::handleMouseButton(int button, int action, int mods) {
  ULMouseButton btn = kMouseButton_None;
  if (button == (0)) btn = kMouseButton_Left;
  else if (button == (1)) btn = kMouseButton_Right;
  else if (button == (2)) btn = kMouseButton_Middle;

  ULMouseEventType type = (action == 1) ? kMouseEventType_MouseDown : kMouseEventType_MouseUp;
  GLFWwindow* window = lveDevice.getWindow().getGLFWwindow();

  double x, y;
  glfwGetCursorPos(window, &x, &y);

  ULMouseEvent evt = ulCreateMouseEvent(type, (int)x, (int)y, btn);
  ulViewFireMouseEvent(view, evt);
  ulDestroyMouseEvent(evt);
}

} // namespace lve
