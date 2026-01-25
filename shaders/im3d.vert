#version 450

layout(location = 0) in vec4 positionSize;
layout(location = 1) in uint color;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform GlobalUbo {
  mat4 projection;
  mat4 view;
  mat4 invView;
  mat4 lightProjectionView;
  vec4 ambientLightColor;
} ubo;

void main() {
    float size = positionSize.w;
    gl_Position = ubo.projection * ubo.view * vec4(positionSize.xyz, 1.0);
    gl_PointSize = size;
    
    // Unpack color (rgba8, MSB = R)
    fragColor = vec4(
        float((color >> 24) & 0xff) / 255.0,
        float((color >> 16) & 0xff) / 255.0,
        float((color >> 8) & 0xff) / 255.0,
        float((color >> 0) & 0xff) / 255.0
    );
}
