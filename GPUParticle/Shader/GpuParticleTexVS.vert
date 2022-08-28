#version 450
#extension GL_GOOGLE_include_directive : enable
#include "Shader/SceneParameter.glsl"
#include "Shader/GpuParticleData.glsl"

layout(location=0) in vec4 inPosition;
layout(location=1) in vec2 inUV0;

layout(location=0) out vec3 outColor;
layout(location=1) out vec2 outUV0;

void main()
{
  uint index = gl_InstanceIndex;
  gl_Position =vec4(-1);
 
  if(gpuParticleElements[index].isActive == 0) {
    return;
  }
  vec4 position = matBillboard * inPosition;
  position += gpuParticleElements[index].position;
  gl_Position = proj * view * position;

  uint colorIndex = gpuParticleElements[index].colorIndex;
  outColor = particleColors[colorIndex].xyz;
  outUV0 = inUV0;
}
