#version 450
#extension GL_GOOGLE_include_directive : enable
#include "Shader/SceneParameter.glsl"
#include "Shader/GpuParticleData.glsl"

layout(location=0) out vec4 outColor;

void main()
{
  uint index = gl_VertexIndex;
  gl_Position =vec4(-1);
  if( index >= maxParticleCount ) {
    return;
  }
 
  GpuParticleElement data = gpuParticleElements[index];
  if( data.isActive == 0 ) {
    return;
  }

  vec4 pos = vec4(1);
  pos.xyz = data.position.xyz;
  gl_Position = proj * view * pos;
  outColor = particleColors[data.colorIndex];
  gl_PointSize = 3;
}
