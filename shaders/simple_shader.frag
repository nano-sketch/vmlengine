#version 450

layout (location = 0) in vec3 fragColor;
layout (location = 1) in vec3 fragPosWorld;
layout (location = 2) in vec3 fragNormalWorld;
layout (location = 3) in vec2 fragUV;
layout (location = 4) in vec4 fragPosLight;

layout (location = 0) out vec4 outColor;

struct PointLight {
  vec4 position; // ignore w
  vec4 color; // w is intensity
};

layout(set = 0, binding = 0) uniform GlobalUbo {
  mat4 projection;
  mat4 view;
  mat4 invView;
  mat4 lightProjectionView;
  vec4 ambientLightColor; // w is intensity
  PointLight pointLights[10];
  int numLights;
} ubo;

layout(set = 1, binding = 0) uniform sampler2D texSampler;
layout(set = 2, binding = 0) uniform sampler2DShadow shadowMap;

layout(push_constant) uniform Push {
  mat4 modelMatrix;
  mat4 normalMatrix;
} push;

/**
 * @brief Calculates shadow factor with a small bias to prevent acne.
 */
float calculateShadow(vec4 shadowCoord) {
  vec3 projCoords = shadowCoord.xyz / shadowCoord.w;
  projCoords.xy = projCoords.xy * 0.5 + 0.5;
  
  if (projCoords.z > 1.0 || projCoords.z < 0.0) return 1.0;

  // Manual bias to handle depth precision
  float bias = 0.0005;
  return textureProj(shadowMap, vec4(projCoords.xy, projCoords.z - bias, 1.0));
}

void main() {
  vec3 surfaceNormal = normalize(fragNormalWorld);
  vec3 cameraPosWorld = ubo.invView[3].xyz;
  vec3 viewDirection = normalize(cameraPosWorld - fragPosWorld);

  float shadow = calculateShadow(fragPosLight);

  // Global Solar Illumination (Non-attenuated light 0)
  vec3 totalDiffuse = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;
  vec3 totalSpecular = vec3(0.0);

  for (int i = 0; i < ubo.numLights; i++) {
    PointLight light = ubo.pointLights[i];
    vec3 directionToLight = light.position.xyz - fragPosWorld;
    float distanceSq = dot(directionToLight, directionToLight);
    directionToLight = normalize(directionToLight);

    // Light 0 is treated as the "Sun" (Directional-like with no attenuation)
    float attenuation = (i == ubo.numLights - 1) ? 1.0 : (1.0 / distanceSq);
    
    float cosAngIncidence = max(dot(surfaceNormal, directionToLight), 0);
    vec3 intensity = light.color.xyz * light.color.w * attenuation;

    // Direct Diffuse
    totalDiffuse += (intensity * cosAngIncidence) * shadow;

    // Specular "Reflection" - Blinn-Phong
    vec3 halfAngle = normalize(directionToLight + viewDirection);
    // Lower exponent = bigger highlights, Higher = sharper
    float blinnTerm = pow(max(dot(surfaceNormal, halfAngle), 0.0), 64.0);
    
    // Add a Fresnel-like boost for "reflection" look on glancing angles
    float fresnel = 0.04 + 0.96 * pow(1.0 - max(dot(surfaceNormal, viewDirection), 0.0), 5.0);
    totalSpecular += (intensity * blinnTerm * fresnel) * shadow;
  }
  
  vec4 texColor = texture(texSampler, fragUV);
  
  // Composite: Scale diffuse by texture, add specular on top (dielectric style)
  vec3 finalColor = (totalDiffuse * fragColor * texColor.rgb) + (totalSpecular * 2.0);
  
  // Subtle "fake GI" bounce from below (negative light pos)
  float bounce = max(dot(surfaceNormal, vec3(0.0, 1.0, 0.0)), 0.0) * 0.05;
  finalColor += bounce * texColor.rgb;

  outColor = vec4(finalColor, texColor.a);
}
