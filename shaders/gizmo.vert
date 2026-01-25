#version 450

// Vertex attributes
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;

// Output to fragment shader
layout(location = 0) out vec3 fragColor;

// Push constants for transformation
layout(push_constant) uniform Push {
  mat4 modelMatrix;
  vec4 color;
} push;

// Global UBO (for camera matrices)
layout(set = 0, binding = 0) uniform GlobalUbo {
  mat4 projection;
  mat4 view;
  mat4 invView;
  mat4 lightProjectionView;
  vec4 ambientLightColor;
} ubo;

void main() {
  // Transform vertex to world space
  vec4 positionWorld = push.modelMatrix * vec4(position, 1.0);
  
  // Project to screen space
  gl_Position = ubo.projection * ubo.view * positionWorld;
  
  // Pass vertex color to fragment shader
  fragColor = color;
}
